<?php

function spcontactform_get_webforms() {
  $webforms = &drupal_static(__FUNCTION__);
  if (!isset($webforms)) {
    $webforms = array();
    $query = new EntityFieldQuery();

    $result = $query
      ->entityCondition('entity_type', 'node')
      ->entityCondition('bundle', 'webform')
      ->execute();

    if (!empty($result['node'])) {
      $nids = array_keys($result['node']);
      $webforms = entity_load('node', $nids);
    }
  }
  // Filter already set forms.
  $sync_data = variable_get('spcontactform_sync_data', array());
  foreach ($webforms as $key => $webform) {
    if (in_array($webform->nid, array_keys($sync_data))) {
      unset($webforms[$key]);
    }
  }
  return $webforms;
}

function spcontactform_get_webform_allowed_components($nid, $allowed_types) {
  if (!empty($nid)) {
    $webform = node_load($nid);

    if (!empty($webform->webform['components'])) {
      foreach ($webform->webform['components'] as $key => $component) {
        if (isset($component['type']) && !empty($allowed_types[$component['type']])) {
          $components_filtered[$key] = $component;
        }
      }
      if (!empty($components_filtered)) {
        return $components_filtered;
      }
    }
  }
  return FALSE;
}

function spcontactform_get_civi_groups($sync = TRUE) {
  if ($sync) {
    $groups = &drupal_static(__FUNCTION__);
    if (!isset($groups)) {

      if ($cache = cache_get('spcontactform_civi_groups')) {
        $groups = $cache->data;
      }
      else {
        set_time_limit(0);
        $civiapi = spciviapi_get_api();
        $groups = array();

        $civiapi->Group->Get(array(
          'sequential'            => 1,
          'option.limit'          => 9999,
        ));
        $groups_results = $civiapi->result();
        if (isset($groups_results->values)) {
          foreach ($groups_results->values as $value) {
            if (
              empty($value->group_type) ||
              in_array('2', $value->group_type)
            ) {
            $groups[$value->id] = $value->title;
            }
          }
          asort($groups);
          variable_set('spcontactform_civi_groups', $groups);
        }
        else {
          drupal_set_message('Error syncing civicrm groups, using saved data.', 'warning');
        }
      }
      $groups = variable_get('spcontactform_civi_groups', array());
      cache_set('spcontactform_civi_groups', $groups, 'cache', time() + 3600);
    }
    return $groups;
  }
  $groups = variable_get('spcontactform_civi_groups', array());
  return $groups;
}

function spcontactform_parse_webform_select_options($webform, $selected_component) {
  $options = array();
  if (!empty($webform->webform['components'][$selected_component]['extra']['items'])) {
    $items = $webform->webform['components'][$selected_component]['extra']['items'];
    $lines = explode("\n", trim($items));
    foreach ($lines as $line) {
      $line = trim($line);
      $matches = array();
      if (preg_match('/^([^|]*)\|(.*)$/', $line, $matches)) {
        $options[$matches[1]] = empty($matches[2]) ? $matches[1] : $matches[2];
      }
    }
  }
  return $options;
}

/**
 * Parses submission data with sync info in raw data array to sync.
 */
function spcontactform_parse_submission($submission, $sync_data) {
  $sync_items = spcontactform_get_data_names(); 
  foreach ($sync_items as $key => $name) {
    if (isset($sync_data[$key . '_component'])) {
      if (!empty($submission->data[$sync_data[$key . '_component']][0])) {
        $var = $submission->data[$sync_data[$key . '_component']][0];
        $var_clean = call_user_func('spcontactform_valid_' . $key, $var);
        if (!empty($var_clean)) {
          $data[$key] = $var_clean;
        }
      }
    }
  }
  return $data;
}

/**
 * Checks wether a submission contains sufficient data to create civi contact.
 */
function spcontactform_check_submission($data) {
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
    drupal_set_message('Contact can be made!');
    return TRUE;
  }
  drupal_set_message('Contact kan niet worden gemaakt!', 'error');
  return FALSE;
}

function spcontactform_sync_contact($data, $sync_data) {
  // Find similar existing contacts in civi.
  $similar_contacts = spcontactform_find_similar_contacts($data, $sync_data);
  // Sync contact.
  if (!empty($similar_contacts)) {
    $contact = spcontactform_add_contact($data, $sync_data);
  }
  else {
    $contact = spcontactform_merge_contact($data, $sync_data, $similar_contacts);
  }
  // Add contact to groups.
  spcontactform_contact_add_groups($submission, $sync_data, $contact);
}

function spcontactform_find_similar_contacts($data, $sync_data) {
  return array();
}

function spcontactform_add_contact($data, $sync_data) {
}

function spcontactform_merge_contact($data, $sync_data, $similar_contacts) {
}

function spcontactform_contact_add_groups($data, $sync_data, $contact) {
}
