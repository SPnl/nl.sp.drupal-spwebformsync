<?php

/* ---------------------------------------------------------------------------
 * SUBMISSION PROCESSING RELATED FUNCTIONS
 * ---------------------------------------------------------------------------
 */

function spwebformsync_process_submission($submission) {
  $sync_data_all = variable_get('spwebformsync_sync_data', array());
  // Check if there is a spwebformsync for this webform.
  if (isset($sync_data_all[$submission->nid])) {
    watchdog('spwebformsync', 'Processing submission nr. ' . $submission->sid);
    $sync_data = $sync_data_all[$submission->nid];
    // Parse data.
    $data = spwebformsync_parse_submission($submission, $sync_data);
    // Check if submission contains data nescesarry to create contact.
    if (spwebformsync_check_submission_sufficient($data)) {
      // Sync contact.
      $contact = spwebformsync_sync_contact($submission, $data, $sync_data);
      if (!empty($contact)) {
        spwebformsync_store_submission_sync_state($submission->nid, $submission->sid, 'synced');
      }
      else {
        spwebformsync_store_submission_sync_state($submission->nid, $submission->sid, 'error');
      }
    }
    else {
      spwebformsync_store_submission_sync_state($submission->nid, $submission->sid, 'synced');
    }
    watchdog('spwebformsync', 'Processed submission nr. '.$submission->sid);
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
 * Checks wether a submission contains sufficient data to create new or sync existing civi contact.
 */
function spwebformsync_check_submission_sufficient($data) {
  // Check if values are sufficient to create new or sync existing civi contact.
  if 
    (
      !empty($data['contact_id']) ||
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
      )
    ) {
    return TRUE;
  }
  watchdog('spwebformsync', '<p>Contact kon niet worden aangemaakt, te weinig data.</p><pre>'.print_r($data, TRUE).'</pre>');
  drupal_set_message('Contact kan niet worden gemaakt!', 'error');
  return FALSE;
}

function spwebformsync_sync_contact($submission, $data, $sync_data) {
  // Find similar existing contacts in civi.
  $same_contacts = spwebformsync_find_same_contacts($data, $submission);
  if ($same_contacts !== FALSE) {
    // Sync contact.
    if (empty($same_contacts) || count($same_contacts) > 1) {
      $contact = spwebformsync_add_contact($data, $sync_data, $submission);
      if (count($same_contacts) > 1) {
        $merge_email = variable_get('spwebformsync_merge_contact_email_address', '');
        if (!empty($merge_email)) {
          // Send mail for manually merging.
          $params = array(
            'contact_id' => $contact,
          );
          $message = drupal_mail('spwebformsync', 'manual_merge', $merge_email, language_default(), $params, 'webmaster@sp.nl');
          if (!empty($message['result'])) { 
            watchdog('spwebformsync', 'Contact handmatig samenvoegen, mail verzonden.');
            drupal_set_message('Contact handmatig samenvoegen, mail verzonden.');
          }
          else {
            watchdog('spwebformsync', 'Contact handmatig samenvoegen, geen mail verzonden verzonden (fout opgetreden).');
            drupal_set_message('Contact handmatig samenvoegen, geen mail verzonden (fout opgetreden).');
          }
        }
        else {
          watchdog('spwebformsync', 'Contact handmatig samenvoegen, geen mail verzonden.');
          drupal_set_message('Contact handmatig samenvoegen, geen mail verzonden.');
        }
      }
    }
    else {
      $contact = spwebformsync_merge_contacts($data, $sync_data, $same_contacts, $submission);
    }
    if (!empty($contact)) {
      // Add contact to groups.
      $result = spwebformsync_contact_add_groups($submission, $sync_data, $contact);
      if (!empty($result)) {
        return $contact;
      }
    }
  }
  return FALSE;
}

function spwebformsync_find_same_contacts($data, $submission) {
  $spcivi = \SPCivi::getInstance();
  set_time_limit(60);
  $same_contacts = array();

  // Find same contact based on contact_id.
  if (isset($data['contact_id'])) {
    $contact_id = $data['contact_id'];
    $params = array(
      'option.limit' => 1,
      'contact_id' => $contact_id,
    );
    $contact_results = $spcivi->api('Contact', 'get', $params);
    if (_spcf_check_result($contact_results, 'een contact', 'get', $params, $submission)) {
      if (!empty($contact_results['values'])) {
        // Check if first name and last name are identical.
        $same_contacts[$contact_id] = $contact_id;
        return $same_contacts;
      }
    }
  }

  // Find same contacts based on mobile phone.
  if (
    isset($data['telephone']) &&
    strpos($data['telephone'], '06') === 0 &&
    (
      isset($data['last_name']) ||
      isset($data['first_name'])
    )
  ) {
    $same_contacts_phone = array();
    $mobile_phone = $data['telephone'];
    $params = array(
      'option.limit' => 5,
    );
    // Also check international number.
    $mobile_phone_int = '0031' . substr($mobile_phone, 1);
    foreach (array($mobile_phone, $mobile_phone_int) as $number_to_check) {
      $params['phone_numeric'] = $number_to_check;
      $mobile_phone_results = $spcivi->api('Phone', 'get', $params);
      if (_spcf_check_result($mobile_phone_results, 'telefoonnummers', 'get', $params, $submission)) {
        if (!empty($mobile_phone_results['values'])) {
          foreach ($mobile_phone_results['values'] as $value) {
            $same_contacts_phone[$value['contact_id']] = $value['contact_id'];
          }
        }
      }
      else {
        return FALSE;
      }
    }
    // check if found contacts have same last name.
    foreach ($same_contacts_phone as $phone_contact_id) {
      // Get contact information.
      $params = array(
        'contact_id' => $phone_contact_id,
      );
      $contact_results = $spcivi->api('Contact', 'get', $params);
      if (_spcf_check_result($contact_results, 'een contact', 'get', $params, $submission)) {
        if (!empty($contact_results['values'])) {
          // Check if first name and last name are identical.
          $contact = array_pop($contact_results['values']);
          if (
            (
              isset($data['last_name']) &&
              strpos(strtolower($contact['display_name']), strtolower($data['last_name'])) !== FALSE
            ) ||
            (
              isset($data['first_name']) &&
              strpos(strtolower($contact['display_name']), strtolower($data['first_name'])) !== FALSE
            )
          ) {
          $same_contacts[$phone_contact_id] = $phone_contact_id;
          }
        }
      }
      else {
        return FALSE;
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
    if (_spcf_check_result($email_results, 'e-mailadressen', 'get', $params, $submission)) {
      if (!empty($email_results['values'])) {
        foreach ($email_results['values'] as $value) {
          $email_contact_ids[$value['contact_id']] = $value['contact_id'];
        }
        foreach ($email_contact_ids as $contact_id) {
          // Get contact information.
          $params = array(
            'contact_id' => $contact_id,
          );
          $contact_results = $spcivi->api('Contact', 'get', $params);
          if (_spcf_check_result($contact_results, 'een contact', 'get', $params, $submission)) {
            if (!empty($contact_results['values'])) {
              // Check if first name and last name are identical.
              $contact = array_pop($contact_results['values']);
              if (
                $data['first_name'] === $contact['first_name'] &&
                $data['last_name'] === $contact['last_name']
              ) {
                $same_contacts[$contact_id] = $contact_id;
              }
            }
          }
          else {
            return FALSE;
          }
        }
      }
    }
    else {
      return FALSE;
    }
  }
  return $same_contacts;
}

function spwebformsync_add_contact($data, $sync_data, $submission) {
  global $base_url;
  $spcivi = \SPCivi::getInstance();
  set_time_limit(60);
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
    'email' => 'email',
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
    $check_name = TRUE; // Create contact first for id.
  }
  unset($sync_items['name']);
  $create_contact_results = $spcivi->api('Contact', 'create', $params);
  if (_spcf_check_result($create_contact_results, 'een contact', 'create', $params, $submission)) {
    $contact_id = $create_contact_results['id'];
    drupal_set_message('Contact in CiviCRM toegevoegd: ' . $contact_id);
    watchdog('spwebformsync', 'Contact in CiviCRM toegevoegd: ' . $contact_id);

    // If no split name, send mail for manually checking name.
    if (!empty($check_name)) {
      $check_name_email = variable_get('spwebformsync_check_name_email_address', '');
      if (!empty($check_name_email)) {
        $params = array(
          'contact_id' => $contact_id,
          'name' => $data['name'],
        );
        $message = drupal_mail('spwebformsync', 'check_name', $check_name_email, language_default(), $params, 'webmaster@sp.nl');
      }
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
      if (!_spcf_check_result($create_phone_result, 'een telefoonnummer', 'create', $params, $submission)) {
        return FALSE;
      }
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
      if (!_spcf_check_result($create_address_results, 'een adres', 'create', $params, $submission)) {
        return FALSE;
      }
    }
    // If available set unverified tag.
    $tag_id = variable_get('spwebformsync_unverified_tag', FALSE);
    if (!empty($tag_id)) {
      $params = array(
        'contact_id' => $contact_id,
        'tag_id' => $tag_id,
      );
      $create_tag_results = $spcivi->api('EntityTag', 'create', $params);
      if (!_spcf_check_result($create_tag_results, 'een tag', 'create', $params, $submission)) {
        return FALSE;
      }
    }
    return $contact_id;
  }
  return FALSE;
}

function spwebformsync_merge_contacts($data, $sync_data, $same_contacts, $submission) {
  $merged_data = FALSE;
  $spcivi = \SPCivi::getInstance();
  set_time_limit(60);

  // Get contact.
  $contact_id = array_pop($same_contacts);
  $params = array(
    'id' => $contact_id,
  );
  $get_contact_results = $spcivi->api('Contact', 'get', $params);
  if (_spcf_check_result($get_contact_results, 'contacten', 'get', $params, $submission)) {
    if (!empty($get_contact_results['values'])) {
      $contact = array_pop($get_contact_results['values']);

      drupal_set_message('Bestaand contact gevonden: ' . $contact_id);
      watchdog('spwebformsync', 'Bestaand contact gevonden: ' . $contact_id);

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
        if(_spcf_check_result($merge_contact_results, 'contacten', 'create', $params, $submission)) {
          $merged_data = TRUE;
        }
        else {
          return FALSE;
        }

      }

      // Merge email data with contact.
      if (!empty($data['email'])) {
        // Check if e-mail is already set.
        $params = array(
          'contact_id' => $contact_id,
          'email' => $data['email']
        );
        $get_emails_result = $spcivi->api('Email', 'get', $params);
        if(!_spcf_check_result($get_emails_result, 'een e-mailadres', 'get', $params, $submission)) {
          return FALSE;
        }
        if (!empty($get_emails_result) && empty($get_emails_result['values'])) {
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
          if (_spcf_check_result($create_email_result, 'een e-mailadres', 'create', $params, $submission)) {
            $merged_data = TRUE;
          }
          else {
            return FALSE;
          }
        }
      }

      // Merge phone data with contact.
      if (!empty($data['telephone'])) {
        // Check if phone is already set.
        $params = array(
          'contact_id' => $contact_id,
          'phone_numeric' => $data['telephone']
        );
        $get_phone_result = $spcivi->api('Phone', 'get', $params);
        if (!_spcf_check_result($get_phone_result, 'een telefoonnummer', 'get', $params, $submission)) {
          return FALSE;
        }
        if (!empty($get_phone_result) && empty($get_phone_result['values'])) {
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
          if(_spcf_check_result($create_phone_result, 'een telefoonnummer', 'create', $params, $submission)) {
            $merged_data = TRUE;
          }
          else {
            return FALSE;
          }
        }
      }

      // Merge address data if no address with same house number or postal code exists.
      if (!empty($data['house_number']) || !empty($data['postal_code'])) {
        if (!empty($data['house_number'])) {
          // Check if address with same house number exists.
          $params = array(
            'contact_id' => $contact_id,
            'street_number' => $data['house_number'],
          );
          $get_street_number_address_result = $spcivi->api('Address', 'get', $params);
          if(!_spcf_check_result($get_street_number_address_result, 'een adres', 'get', $params, $submission)) {
            return FALSE;
          }
        }
        if (!empty($data['postal_code'])) {
          // Check if address with same postal code exists.
          $params = array(
            'contact_id' => $contact_id,
            'postal_code' => $data['postal_code'],
          );
          $get_postal_code_address_result = $spcivi->api('Address', 'get', $params);
          if(!_spcf_check_result($get_postal_code_address_result, 'een adres', 'get', $params, $submission)) {
            return FALSE;
          }
        }
        if (
            empty($get_street_number_address_result['values']) &&
            empty($get_postal_code_address_result['values'])
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
            if(_spcf_check_result($create_address_results, 'een adres', 'create', $params, $submission)) {
              $merged_data = TRUE;
            }
            else {
              return FALSE;
            }
          }
        }
      }
      if ($merged_data) {
        drupal_set_message('Contactinformatie in CiviCRM samengevoegd met bestaand contact: ' . $contact_id);
        watchdog('spwebformsync', 'Contactinformatie in CiviCRM samengevoegd met bestaand contact: ' . $contact_id);
      }
      return $contact_id;
    }
  }
  return FALSE;
}

function spwebformsync_contact_add_groups($submission, $sync_data, $contact) {
  $added_to_groups = FALSE;
  $spcivi = \SPCivi::getInstance();
  set_time_limit(60);
  $groups = array();
  // Hou op de hoogte van sp nieuws.
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
  // Vaste groepen.
  if (!empty($sync_data['selected_fixed_groups'])) {
    $groups = $groups + $sync_data['selected_fixed_groups'];
  }
  // Variabele groepen.
  $submission_data = $submission->data;
  if (!empty($sync_data['selected_variable_groups'])) {
    foreach ($sync_data['selected_variable_groups'] as $sync_values) {
      $sync_component_id = $sync_values['component_id'];
      if (!empty($submission_data[$sync_component_id])) {
        foreach ($sync_values['values'] as $sync_field_group_data) {
          if (in_array($sync_field_group_data['value_id'], $submission_data[$sync_component_id])) {
            $groups[$sync_field_group_data['group_id']] = $sync_field_group_data['group_id'];
          }
        }
      }
    }
  }
  // Sync met CiviCRM.
  foreach ($groups as $key => $name) {
    $params = array(
      'contact_id' => $contact,
      'group_id' => $key,
    );
    $create_group_membership = $spcivi->api('GroupContact', 'create', $params);
    if (_spcf_check_result($create_group_membership, 'groepslidmaatschappen', 'create', $params, $submission)) {
      $added_to_groups = TRUE;
      $added_groups[] = $key;
    }
    else {
      return FALSE;
    }
  }
  if ($added_to_groups) {
    $groups_text = implode(', ', $added_groups);
    watchdog('spwebformsync', 'Contact nr. '.$contact.' was added to groups ('.$groups_text.').');
  }
  return TRUE;
}

function spwebformsync_split_house_number($house_number) {
  $house_number = trim($house_number);
  $regex = "@^([0-9]+)[!0-9](.*)$@";
  if (preg_match($regex, $house_number, $matches)) {
    return $matches;
  }
  return FALSE;
}

function spwebformsync_split_name($name) {
  $matches = array();
  $name = trim($name);

  // Replace terms.
  $replace = array(
    ' v. ' => ' van ',
    ' vd. ' => ' van de ',
    ' vd ' => ' van de ',
  );
  foreach ($replace as $orig => $new) {
    if (strpos($name, ' '.$orig.' ') !== FALSE) {
      $name = str_replace($orig, $new, $name);
    }
  }

  // Regexes for title and middle names.
  $titles_regex = 'Mr|mr|Mevr|mevr|Dhr|dhr|Ir|ir|Ing|ing|Bc|bc|Ms|ms|Drs|drs|Dr|dr';

  $middle_names_array = spwebformsync_middle_names(); 
  $middle_names_regex = implode('|', $middle_names_array);

  // Split name on middle_name.
  $regex = "@^(?'title'(\b(" . $titles_regex . ")\b\.?\s)?)(?'first_name'(([A-ZÀÈÌÒÙÁÉÍÓÚÝÂÊÎÔÛÃÑÕÄËÏÖÜŸÇÅa-zàèìòùáéíóúýâêîôûãñõäëïöü¡¿çãñõ'\-\.]+)\s*)+?)(?'middle_name'(\b(" . $middle_names_regex . ")\b\s*))(?'last_name'(\b[A-ZÀÈÌÒÙÁÉÍÓÚÝÂÊÎÔÛÃÑÕÄËÏÖÜŸÇÅa-zàèìòùáéíóúýâêîôûãñõäëïöü¡¿çãñõ'][a-zàèìòùáéíóúýâêîôûãñõäëïöü¡¿çãñõ'\-]+\b\s?)+)$@";
  $result = preg_match($regex, $name, $matches);
  if (!preg_match($regex, $name, $matches)) {
    // Split name without middle_name in first name and last name(s).
    $regex = "@^(?'title'(\b(" . $titles_regex . ")\b\.?\s)?)(?'first_name'(([A-ZÀÈÌÒÙÁÉÍÓÚÝÂÊÎÔÛÃÑÕÄËÏÖÜŸÇÅa-zàèìòùáéíóúýâêîôûãñõäëïöü¡¿çãñõ'\-\.]+)\s*)+?)(?'last_name'(\b[A-ZÀÈÌÒÙÁÉÍÓÚÝÂÊÎÔÛÃÑÕÄËÏÖÜŸÇÅa-zàèìòùáéíóúýâêîôûãñõäëïöü¡¿çãñõ'][a-zàèìòùáéíóúýâêîôûãñõäëïöü¡¿çãñõ'\-]+\b\s?)+)$@";
    $result = preg_match($regex, $name, $matches);
    if (!preg_match($regex, $name, $matches)) {
      // Check if only first name.
      $regex = "@^(?'first_name'([A-ZÀÈÌÒÙÁÉÍÓÚÝÂÊÎÔÛÃÑÕÄËÏÖÜŸÇÅa-zàèìòùáéíóúýâêîôûãñõäëïöü¡¿çãñõ'][a-zàèìòùáéíóúýâêîôûãñõäëïöü¡¿çãñõ'\-]+))$@";
      $result = preg_match($regex, $name, $matches);
      if (!preg_match($regex, $name, $matches)) {
        // No match found.
        return FALSE;
      }
    }
  }
  // Set first character uppercase.
  $parts = array(
    'first_name',
    'last_name',
  );
  foreach ($parts as $part) {
    if (!empty($matches[$part])) {
      $matches[$part] = ucfirst($matches[$part]);
    }
  }
  return $matches;
}

function _spcf_check_result($result, $dataname, $action, $data = NULL, $submission = NULL) {
  $action_name = ($action === 'get') ? 'ophalen' : 'aanmaken';
  $place_name = ($action === 'get') ? 'uit' : 'in';
  if (!isset($result['is_error']) || $result['is_error'] == 1) {
    if (empty($submission)) {
      drupal_set_message('Fout bij het ' . $action_name . ' van ' . $dataname . '.', 'error');
      watchdog('spcivisync', '<p>Fout bij het ' . $action_name . ' van ' . $dataname . '.<pre>' . print_r($result, TRUE) . '</pre>Parameters:<pre>' . print_r($data, TRUE) . '</pre>', array(), WATCHDOG_ERROR);
    }
    else {
      drupal_set_message('Fout bij het ' . $action_name . ' van ' . $dataname . ' ' . $place_name . ' CiviCRM (sid = ' . $submission->sid . ').', 'error');
      watchdog('spcivisync', '<p>Fout bij het ' . $action_name . ' van ' . $dataname . ' ' . $place_name . ' CiviCRM (sid = ' . $submission->sid . ').</p>Resultaat:<pre>' . print_r($result, TRUE) . '</pre>Parameters:<pre>' . print_r($data, TRUE) . '</pre>', array(), WATCHDOG_ERROR);
    }
    spwebformsync_store_submission_sync_state($submission->nid, $submission->sid, 'error');
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

/*
 * Stores submission sync state.
 */
function spwebformsync_store_submission_sync_state($nid, $sid, $state) {
  $states = array(
    'unprocessed' => '0',
    'synced' => '1',
    'error' => '2',
  );
  if (!empty($nid) && !empty($sid) && isset($states[$state])) {
    $new_state = $states[$state];
    // Check if state exists.
    $query = "SELECT state FROM {spwebformsync_syncstate} WHERE nid = :nid AND sid = :sid";
    $result = db_query($query, array(':nid' => $nid, ':sid' => $sid));
    $old_state = $result->fetchField(0);
    if ($old_state !== FALSE) {
      if ($old_state != $new_state) {
        // Update existing state.
        $updated = db_update('spwebformsync_syncstate')
          ->fields(array(
            'state' => $new_state,
          ))
          ->condition('sid', $sid)
          ->condition('nid', $nid)
          ->execute();
        if (!empty($updated)) {
          return TRUE;
        }
      }
    }
    else {
      // Insert new state.
      $id = db_insert('spwebformsync_syncstate')
        ->fields(array(
          'nid' => $nid,
          'sid' => $sid,
          'state' => $new_state,
        ))
        ->execute();
      if (!empty($id)) {
        return TRUE;
      }
    }
  }
  return FALSE;
}

function spwebformsync_get_unprocessed_submission_id($nid) {
  if (!empty($nid)) {
    $do_not_check_confirmation = variable_get('spwebformsync_do_not_check_confirmation', 0);

    // Create query.
    $query = db_select('spwebformsync_syncstate', 'state');
    if ($do_not_check_confirmation) {
      $result = $query
        ->fields('state', array('sid'))
        ->condition('state.state', 0)
        ->condition('state.nid', $nid)
        ->range(0,1)
        ->execute();
    }
    else {
      $query->join('webform_submissions', 'sub', 'state.sid = sub.sid');
      $result = $query
        ->fields('state', array('sid'))
        ->condition('sub.confirmed', 1)
        ->condition('state.state', 0)
        ->condition('state.nid', $nid)
        ->range(0,1)
        ->execute();
    }
    $sid = $result->fetchField(0);
    if (!empty($sid)) {
      return $sid;
    }
  }
  return FALSE;
}

function spwebformsync_delete_all_webform_submissions($nid) {
  $result = db_delete('spwebformsync_syncstate')
    ->condition('nid', $nid)
    ->execute();
  return $result;
}

/*
 * Syncs not yet synced webform submissions.
 */
function spwebformsync_sync() {
  $max_cron_number = variable_get('spwebformsync_cron_number', 0);
  $sync_data = variable_get('spwebformsync_sync_data', FALSE);
  $processed = array();
  if (!empty($sync_data)) {
    foreach ($sync_data as $webform_id => $data) {
      for ($i = 1; $i <= $max_cron_number; $i ++) {
        $submission_id = spwebformsync_get_unprocessed_submission_id($webform_id); 
        if (empty($submission_id)) {
          break;
        }
        $submission = webform_get_submission($webform_id, $submission_id);
        if (!empty($submission)) {
          spwebformsync_process_submission($submission);
          $processed[$submission_id] = $submission_id;
        }
      }
    }
  }
  return $processed;
}
