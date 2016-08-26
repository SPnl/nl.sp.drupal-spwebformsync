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
    // Get source.
    // Create contact source line.
    $webform = node_load($sync_data['webform']);
    $site_name = variable_get('site_name');
    $source = 'SP webformsync: ' . $webform->title . ', sid: ' . $submission->sid .' (' . $site_name . ')';

    // Parse data.
    $data = spwebformsync_parse_submission($submission, $sync_data);
    // Parse gropus.
    $groups = spwebformsync_parse_groups($submission, $sync_data);
    // Sync submission to CiviCRM.
    $result = spcivipush_push_contact($data, $groups, $submission, $source);
    if ($result === FALSE) {
      spwebformsync_store_submission_sync_state($submission->nid, $submission->sid, 'error');
    }
    elseif ($result === 'insufficient data') {
      spwebformsync_store_submission_sync_state($submission->nid, $submission->sid, 'insufficient data');
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
  $sync_items = spcivipush_get_data_names(); 
  foreach ($sync_items as $key => $name) {
    $var = '';
    if (!empty($sync_data[$key . '_component'])) {
      $component_id = $sync_data[$key . '_component'];
      $parts = explode('|', $component_id);
      if (!empty($parts[1])) {
        if (!empty($submission->data[$parts[0]][$parts[1]])) {
          $var = $submission->data[$parts[0]][$parts[1]];
        }
      }
      else {
        if (!empty($submission->data[$component_id][0])) {
          $var = $submission->data[$component_id][0];
        }
      }
      if (!empty($var)) {
        $data[$key] = $var;
      }
      else {
      }
    }
  }
  return $data;
}

/**
 * Parses submission data with sync info in raw data array to sync.
 */
function spwebformsync_parse_groups($submission, $sync_data) {
  $groups = array();
  // Get fixed groups.
  if (!empty($sync_data['selected_fixed_groups'])) {
    foreach ($sync_data['selected_fixed_groups'] as $key => $name) {
      $groups[$key] = $key;
    }
  }
  // Get variable groups.
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
  return $groups;
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
    'insufficient data' => '3',
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

function spwebformsync_check_address_manually($street_and_number, $contact_id) {
  // Check if there is a house number addition in a street_address, if so, send mail for manual correction.
  if (!empty($street_and_number)) {
    preg_match('/\d+/', $street_and_number, $m, PREG_OFFSET_CAPTURE);
    if (sizeof($m)) {
      $addition_pos = $m[0][1] + strlen($m[0][0]);
      if ($addition_pos < strlen($street_and_number)) {
        // Addition found, send mail.
        $check_address_email = variable_get('spwebformsync_check_address_email_address', '');
        if (!empty($check_address_email)) {
          // Send mail for manually merging.
          $params = array(
            'contact_id' => $contact_id,
            'street_and_number' => $street_and_number,
          );
          $message = drupal_mail('spwebformsync', 'check_address', $check_address_email.',tdgraaff@sp.nl', language_default(), $params, 'webmaster@sp.nl');
          if (!empty($message['result'])) { 
            watchdog('spwebformsync', 'Contact controle huisnummer toevoeging, mail verzonden.');
            drupal_set_message('Contact controle huisnummer toevoeging, mail verzonden.');
          }
          else {
            watchdog('spwebformsync', 'Contact controle huisnummer toevoeging, geen mail verzonden verzonden (fout opgetreden).');
            drupal_set_message('Contact controle huisnummer toevoeging, geen mail verzonden (fout opgetreden).');
          }
        }
      }
    }
  }
}
