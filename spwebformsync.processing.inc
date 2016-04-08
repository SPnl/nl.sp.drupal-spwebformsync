<?php

/* ---------------------------------------------------------------------------
 * SUBMISSION PROCESSING RELATED FUNCTIONS
 * ---------------------------------------------------------------------------
 */

function spwebformsync_process_submission($submission) {
  $sync_data_all = variable_get('spwebformsync_sync_data', array());
  // Check if there is a spwebformsync for this webform.
  if (isset($sync_data_all[$submission->nid])) {
    $sync_data = $sync_data_all[$submission->nid];
    // Parse data.
    $data = spwebformsync_parse_submission($submission, $sync_data);
    // Check if submission contains data nescesarry to create contact.
    if (spwebformsync_check_submission_sufficient($data)) {
      // Sync contact.
      $contact = spwebformsync_sync_contact($submission, $data, $sync_data);
    }
  }
}

/**
 * Parses submission data with sync info in raw data array to sync.
 */
function spwebformsync_parse_submission($submission, $sync_data) {
  $data = array();
  $sync_items = spwebformsync_get_data_names(); 
  foreach ($sync_items as $key => $name) {
    if (isset($sync_data[$key . '_component'])) {
      if (!empty($submission->data[$sync_data[$key . '_component']][0])) {
        $var = $submission->data[$sync_data[$key . '_component']][0];
        $var_clean = call_user_func('spwebformsync_valid_' . $key, $var);
        if (!empty($var_clean)) {
          $data[$key] = $var_clean;
        }
      }
    }
  }
  // If possible, split house number in house_number and addition.
  if (!empty($data['house_number'])) {
    $split_house_number = spwebformsync_split_house_number($data['house_number']);
    if (!empty($split_house_number[2])) {
      $data['house_number'] = $split_house_number[1];
      if (empty($data['house_number_addition'])) {
        $data['house_number_addition'] = $split_house_number[2];
      }
    }
  }
  // If name item, then if possible, set first name, middle_name, and last name.
  if (!empty($data['name'])) {
    // Check if name contains prepositon.
    $split_name = spwebformsync_split_name($data['name']);
    $parts = array('first_name', 'middle_name', 'last_name');
    foreach ($parts as $part) {
      if (isset($split_name[$part])) {
        unset($data['name']);
        $data[$part] = $split_name[$part];
      }
    }
  }
  return $data;
}

/**
 * Checks wether a submission contains sufficient data to create civi contact.
 */
function spwebformsync_check_submission_sufficient($data) {
  // Check if values are sufficient to create civi contact.
  if 
    (
      !empty($data['email']) ||
      !empty($data['telephone']) ||
      (
        !empty($data['street_and_number']) &&
        !empty($data['locality'])

      ) ||
      (
        !empty($data['house_number']) &&
        (
          (
            !empty($data['street']) &&
            !empty($data['locality'])
          ) ||
          !empty($data['postal_code'])
        )
      )
    ) {
    return TRUE;
  }
  drupal_set_message('Contact kan niet worden gemaakt!', 'error');
  return FALSE;
}

function spwebformsync_sync_contact($submission, $data, $sync_data) {
  // Find similar existing contacts in civi.
  $same_contacts = spwebformsync_find_same_contacts($data);
  // Sync contact.
  if (empty($same_contacts) || count($same_contacts) > 1) {
    $contact = spwebformsync_add_contact($data, $sync_data);
  }
  else {
    $contact = spwebformsync_merge_contacts($data, $sync_data, $same_contacts);
  }
  if (!empty($contact)) {
    // Add contact to groups.
    spwebformsync_contact_add_groups($submission, $sync_data, $contact);
  }
}

function spwebformsync_find_same_contacts($data) {
  $spcivi = \SPCivi::getInstance();
  $same_contacts = array();

  // Find same contacts based on mobile phone.
  if (isset($data['telephone']) && strpos($data['telephone'], '06') === 0) {
    $mobile_phone = $data['telephone'];
    $params = array(
      'option.limit' => 5,
    );
    // Also check international number.
    $mobile_phone_int = '0031' . substr($mobile_phone, 1);
    foreach (array($mobile_phone, $mobile_phone_int) as $number_to_check) {
      $params['phone_numeric'] = $number_to_check;
      $mobile_phone_results = $spcivi->api('Phone', 'get', $params);
      if (_spcf_check_result($mobile_phone_results, 'telefoonnummers', 'get')) {
        if (!empty($mobile_phone_results['values'])) {
          foreach ($mobile_phone_results['values'] as $value) {
            $same_contacts[$value['contact_id']] = $value['contact_id'];
          }
        }
      }
    }
  }

  // Find similar contacts based on email, first- and last name.
  if (
    isset($data['email']) &&
    isset($data['first_name']) &&
    isset($data['last_name'])
  ) {
    $params = array(
      'option.limit' => 5,
      'email' => $data['email'],
    );
    $email_results = $spcivi->api('Email', 'get', $params);
    if (_spcf_check_result($email_results, 'e-mailadressen', 'get')) {
      if (!empty($email_results['values'])) {
        foreach ($email_results['values'] as $value) {
          $email_contact_ids[$value['contact_id']] = $value['contact_id'];
        }
        foreach ($email_contact_ids as $contact_id) {
          // Get contact information.
          $params = array(
            'contact_id' => $contact_id,
          );
          $contact_results = $spcivi->api('Contact', 'getsingle', $params);
          if (_spcf_check_result($contact_results, 'contacten', 'get')) {
            if (!empty($contact_results)) {
              // Check if first name and last name are identical.
              if (
                $data['first_name'] === $contact_results['first_name'] &&
                $data['last_name'] === $contact_results['last_name']
              ) {
                $same_contacts[$value['contact_id']] = $value['contact_id'];
              }
            }
          }
        }
      }
    }
  }
  return $same_contacts;
}

function spwebformsync_add_contact($data, $sync_data) {
  global $base_url;
  $spcivi = \SPCivi::getInstance();
  $sync_items = spwebformsync_get_data_names(); 

  // Create contact source line.
  $webform = node_load($sync_data['webform']);
  $site_name = variable_get('site_name');
  // Create contact.
  // Autosync fields than can be autosynced.
  $autosync = array(
    'first_name' => 'first_name',
    'middle_name' => 'middle_name',
    'last_name' => 'last_name',
  );
  $params = array(
    'contact_type' => 'Individual',
    'contact_source' => 'SP webformsync: ' . $webform->title . ' (' . $site_name . ')',
  );
  foreach ($autosync as $key => $civikey) {
    if (!empty($data[$key])) {
      $params[$civikey] = $data[$key];
      unset($sync_items[$key]);
    }
  }
  // Make sure last name is set, if data is available.
  if (empty($data['first_name']) && empty($data['last_name']) && !empty($data['name'])) {
    $params['last_name'] = $data['name'];
  }
  unset($sync_items['name']);
  $create_contact_results = $spcivi->api('Contact', 'create', $params);
  if (_spcf_check_result($create_contact_results, 'contacten', 'create')) {
    $contact_id = $create_contact_results['id'];
    drupal_set_message('Contact toegevoegd: ' . $contact_id);

    // If available, add email.
    if (!empty($data['email'])) {
      $params = array(
        'contact_id' => $contact_id,
        'location_type_id' => 3,
        'email' => $data['email'],
        'is_primary' => 1,
      );
      $create_email_result = $spcivi->api('Email', 'create', $params);
      _spcf_check_result($create_email_result, 'e-mailadressen', 'create');
    }
    // If available, add phone.
    if (!empty($data['telephone'])) {
      if (strpos($data['telephone'], '06') === 0) {
        $phone_type_id = 2;
      }
      else {
        $phone_type_id = 1;
      }
      $params = array(
        'contact_id' => $contact_id,
        'location_type_id' => 3,
        'phone' => $data['telephone'],
        'is_primary' => 1,
        'phone_type_id' => $phone_type_id,
      );
      $create_phone_result = $spcivi->api('Phone', 'create', $params);
      _spcf_check_result($create_phone_result, 'telefoonnummers', 'create');
    }
    // If available, add address.
    $autosync = array(
      'street_and_number' => 'street_address',
      'street' => 'street_name',
      'house_number' => 'street_number',
      'house_number_addition' => 'street_unit',
      'postal_code' => 'postal_code',
      'locality' => 'city',
    );
    $params = array(
      'contact_id' => $contact_id,
      'location_type_id' => 1,
      'is_primary' => 1,
      'country_id' => 1152,
    );
    $address_data_found = FALSE;
    foreach ($autosync as $key => $civikey) {
      if (!empty($data[$key])) {
        $params[$civikey] = $data[$key];
        $address_data_found = TRUE;
      }
    }
    if ($address_data_found) {
      $create_address_results = $spcivi->api('Address', 'create', $params);
      _spcf_check_result($create_address_results, 'adressen', 'create');
    }
    // If available set unverified tag.
    $tag_id = variable_get('spwebformsync_unverified_tag', FALSE);
    if (!empty($tag_id)) {
      $params = array(
        'contact_id' => $contact_id,
        'tag_id' => $tag_id,
      );
      $create_tag_results = $spcivi->api('EntityTag', 'create', $params);
      _spcf_check_result($create_tag_results, 'tags', 'create');
    }
    return $contact_id;
  }
  return FALSE;
}

function spwebformsync_merge_contacts($data, $sync_data, $same_contacts) {
  $spcivi = \SPCivi::getInstance();

  // Get contact.
  $contact_id = array_pop($same_contacts);
  $params = array(
    'id' => $contact_id,
  );
  $get_contact_results = $spcivi->api('Contact', 'get', $params);
  if (_spcf_check_result($get_contact_results, 'contacten', 'get')) {
    if (!empty($get_contact_results['values'])) {
      $contact = array_pop($get_contact_results['values']);

      // Merge name data with contact.
      if (
        empty($contact['first_name']) &&
        empty($contact['last_name']) &&
        (
          !empty($data['first_name']) ||
          !empty($data['last_name']) ||
          !empty($data['name'])
        )
      ) {
        $params = array(
          'id' => $contact_id,
        );
        $autosync = array(
          'first_name' => 'first_name',
          'middle_name' => 'middle_name',
          'last_name' => 'last_name',
        );
        foreach ($autosync as $key => $civikey) {
          if (!empty($data[$key])) {
            $params[$civikey] = $data[$key];
          }
        }
        // Make sure last name is set, if data is available.
        if (empty($data['first_name']) && empty($data['last_name']) && !empty($data['name'])) {
          $params['last_name'] = $data['name'];
        }
        $merge_contact_results = $spcivi->api('Contact', 'create', $params);
        _spcf_check_result($merge_contact_results, 'contacten', 'create');
      }

      // Merge email data with contact.
      if (!empty($data['email'])) {
        // Check if e-mail is already set.
        $params = array(
          'contact_id' => $contact_id,
          'email' => $data['email']
        );
        $get_email_result = $spcivi->api('Email', 'getsingle', $params);
        if (!empty($get_email_result) && empty($get_email_result['id'])) {
          if (empty($contact['email'])) {
            $primary = 1;
            $location_type_id = 3;
          }
          else {
            $primary = 0;
            $location_type_id = 4;
          }
          // Add email.
          $params = array(
            'contact_id' => $contact_id,
            'location_type_id' => $location_type_id,
            'email' => $data['email'],
            'is_primary' => $primary,
          );
          $create_email_result = $spcivi->api('Email', 'create', $params);
          _spcf_check_result($create_email_result, 'e-mailadressen', 'create');
        }
      }

      // Merge phone data with contact.
      if (!empty($data['telephone'])) {
        // Check if phone is already set.
        $params = array(
          'contact_id' => $contact_id,
          'phone_numeric' => $data['telephone']
        );
        $get_phone_result = $spcivi->api('Phone', 'getsingle', $params);
        if (!empty($get_phone_result) && empty($get_phone_result['id'])) {
          // Phone doesn't exist, add.
          if (empty($contact['phone'])) {
            $primary = 1;
            $location_type_id = 3;
          }
          else {
            $primary = 0;
            $location_type_id = 4;
          }
          if (strpos($data['telephone'], '06') === 0) {
            $phone_type_id = 2;
          }
          else {
            $phone_type_id = 1;
          }
          $params = array(
            'contact_id' => $contact_id,
            'location_type_id' => $location_type_id,
            'phone' => $data['telephone'],
            'is_primary' => $primary,
            'phone_type_id' => $phone_type_id,
          );
          $create_phone_result = $spcivi->api('Phone', 'create', $params);
          _spcf_check_result($create_phone_result, 'telefoonnummers', 'create');
        }
      }

      // Merge address data if no address with same house number or postal code exists.
      if (!empty($data['house_number'])) {
        // Check if address with same house number exists.
        $params = array(
          'contact_id' => $contact_id,
          'street_number' => $data['house_number'],
        );
        $get_street_number_address_result = $spcivi->api('Address', 'getsingle', $params);
      }
      if (!empty($data['postal_code'])) {
        $params = array(
          'contact_id' => $contact_id,
          'postal_code' => $data['postal_code'],
        );
        $get_postal_code_address_result = $spcivi->api('Address', 'getsingle', $params);
      }
      if (
          empty($get_street_number_address_result['id']) &&
          empty($get_postal_code_address_result['id'])
      ) {
        // Add extra address.
        if (empty($contact['address_id'])) {
          $primary = 1;
          $location_type_id = 3;
        }
        else {
          $primary = 0;
          $location_type_id = 4;
        }
        $autosync = array(
          'street_and_number' => 'street_address',
          'street' => 'street_name',
          'house_number' => 'street_number',
          'house_number_addition' => 'street_unit',
          'postal_code' => 'postal_code',
          'locality' => 'city',
        );
        $params = array(
          'contact_id' => $contact_id,
          'location_type_id' => $location_type_id,
          'is_primary' => $primary,
          'country_id' => 1152,
        );
        $address_data_found = FALSE;
        foreach ($autosync as $key => $civikey) {
          if (!empty($data[$key])) {
            $params[$civikey] = $data[$key];
            $address_data_found = TRUE;
          }
        }
        if ($address_data_found) {
          $create_address_results = $spcivi->api('Address', 'create', $params);
          _spcf_check_result($create_address_results, 'adressen', 'create');
        }
      }
      return $contact_id;
    }
  }
}

function spwebformsync_contact_add_groups($submission, $sync_data, $contact) {

  $spcivi = \SPCivi::getInstance();
  $groups = array();
  if (!empty($sync_data['hou_hoog_component'])) {
    if (!empty($submission->data[$sync_data['hou_hoog_component']][0])) {
      $selected_hou_hoog_value = $submission->data[$sync_data['hou_hoog_component']][0];
      $hou_hoog_accept_value = $sync_data['hou_hoog_accept_value'];
      if ($selected_hou_hoog_value == $hou_hoog_accept_value) {
        $hou_hoog_group_id = variable_get('spwebformsync_hou_hoog_group', FALSE);
        if (!empty($hou_hoog_group_id)) {
          $groups[$hou_hoog_group_id] = $hou_hoog_group_id;
        }
      }
    }
  }
  if (!empty($sync_data['selected_groups'])) {
    $groups = $groups + $sync_data['selected_groups'];
  }
  foreach ($groups as $key => $name) {
    $params = array(
      'contact_id' => $contact,
      'group_id' => $key,
    );
    $create_group_membership = $spcivi->api('GroupContact', 'create', $params);
    _spcf_check_result($create_group_membership, 'groepslidmaatschappen', 'create');
  }
}

function spwebformsync_split_house_number($house_number) {
  $house_number = trim($house_number);
  $regex = "@^([0-9]+) *(.*)$@";
  if (preg_match($regex, $house_number, $matches)) {
    return $matches;
  }
  return FALSE;
}

function spwebformsync_split_name($name) {
  $name = trim($name);
  // If name contains only lowercase words, make all first characters uppercase.
  $spaceless = str_replace(' ', '', $name);
  if (ctype_lower($spaceless)) {
    $name = ucwords($name);
  }
  // Split name on middle_name.
  $middle_names_array = spwebformsync_middle_names(); 
  $middle_names_regex = implode('|', $middle_names_array);

  $regex = "@^((?'first_name'(\b([A-ZÀÈÌÒÙÁÉÍÓÚÝÂÊÎÔÛÃÑÕÄËÏÖÜŸÇÅ.]+|[A-ZÀÈÌÒÙÁÉÍÓÚÝÂÊÎÔÛÃÑÕÄËÏÖÜŸÇÅ][a-zàèìòùáéíóúýâêîôûãñõäëïöü¡¿çãñõ'\-]+)\s*)+?)\s)(?'middle_name'(\b(" . $middle_names_regex . ")\b\s*)+)\s+(?'last_name'(\b[A-ZÀÈÌÒÙÁÉÍÓÚÝÂÊÎÔÛÃÑÕÄËÏÖÜŸÇÅ][a-zàèìòùáéíóúýâêîôûãñõäëïöü¡¿çãñõ'-]+\b).*)$@";
  if (preg_match($regex, $name, $matches)) {
    return $matches;
  }
  // Split name without middle_name in first name and last name(s).
  $regex = "@^(?'first_name'(\b([A-ZÀÈÌÒÙÁÉÍÓÚÝÂÊÎÔÛÃÑÕÄËÏÖÜŸÇÅ.]+|[A-ZÀÈÌÒÙÁÉÍÓÚÝÂÊÎÔÛÃÑÕÄËÏÖÜŸÇÅ][a-zàèìòùáéíóúýâêîôûãñõäëïöü¡¿çãñõ'\-]+)))\s+(?'last_name'(\b[A-ZÀÈÌÒÙÁÉÍÓÚÝÂÊÎÔÛÃÑÕÄËÏÖÜŸÇÅ][a-zàèìòùáéíóúýâêîôûãñõäëïöü¡¿çãñõ'-]+\b\s*)+)$@";
  if (preg_match($regex, $name, $matches)) {
    return $matches;
  }
  // Check if only first name.
  $regex = "@^(?'first_name'(\b[A-ZÀÈÌÒÙÁÉÍÓÚÝÂÊÎÔÛÃÑÕÄËÏÖÜŸÇÅ.]+|[A-ZÀÈÌÒÙÁÉÍÓÚÝÂÊÎÔÛÃÑÕÄËÏÖÜŸÇÅ.][a-zàèìòùáéíóúýâêîôûãñõäëïöü¡¿çãñõ'\-]+\b))$@";
  if (preg_match($regex, $name, $matches)) {
    return $matches;
  }
  return FALSE;
}

function _spcf_check_result($result, $dataname, $action) {
  $action_name = ($action === 'get') ? 'ophalen' : 'aanmaken';
  if (!isset($result['is_error']) || $result['is_error'] == 1) {
    drupal_set_message('Fout bij het ' . $action_name . ' van ' . $dataname . '.', 'error');
    return FALSE;
  }
  return TRUE;
}

function spwebformsync_check_submission_confirmed($sid) {
  $result = db_query("SELECT confirmed FROM {webform_submissions} WHERE sid = :sid", array('sid' => $sid));
  if (!empty($result->fetchField(0))) {
    return TRUE;
  }
  return FALSE;
}
