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

    // Process spreferal.
    if (!empty($sync_data['sprefer_field']) && !empty($sync_data['sprefer_group'])) {
      $error = FALSE;
      // Process SP Referal.
      $referal_id = $submission->data[$sync_data['sprefer_field']][0];
      $data = array('contact_id' => $referal_id);
      $groups = array($sync_data['sprefer_group'] => $sync_data['sprefer_group']);
      $result = spcivipush_push_contact($data, $groups, $submission, $source);
      $error = spwebformsync_push_contact_process_result($result, $submission, $sync_data); 
      if ($error) {
        watchdog('spwebformsync', 'Processed submission nr. ' . $submission->sid);
        return;
      }
    }

    // Process contact data.
    // Parse data.
    $data = spwebformsync_parse_submission($submission, $sync_data);
    // Parse gropus.
    $groups = spwebformsync_parse_groups($submission, $sync_data);
    // Sync submission to CiviCRM.
    $result = spcivipush_push_contact($data, $groups, $submission, $source);
    $error = spwebformsync_push_contact_process_result($result, $submission, $sync_data); 
    if (!$error) {
      $contact_id = $result;
      spwebformsync_store_submission_sync_state($submission->nid, $submission->sid, 'synced');
      spwebformsync_send_mails($contact_id, $sync_data, $submission);
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
    'contact id not found' => '4',
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
  $items_found = TRUE;
  if (!empty($sync_data)) {
    $number_of_webforms_to_sync = count($sync_data);
    $count = 0;
    while ($count < $max_cron_number && $items_found) {
      $items_found = FALSE;
      foreach ($sync_data as $webform_id => $data) {
        $submission_id = spwebformsync_get_unprocessed_submission_id($webform_id);
        if (empty($submission_id)) {
          continue;
        }
        $submission = webform_get_submission($webform_id, $submission_id);
        if (!empty($submission)) {
          $items_found = TRUE;
          $count ++;
          spwebformsync_process_submission($submission);
          $processed[$submission_id] = $submission_id;
        }
        if ($count >= $max_cron_number) {
          break;
        }
        sleep(1);
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

function spwebformsync_send_mails($contact_id, $sync_data, $submission) {
  if (!empty($sync_data)) {
    if (!empty($sync_data['fixed_mail_text']['value']) || !empty($sync_data['variable_mail'])) {
      // Get contact data from CiviCRM.
      $spcivi = \SPCivi::getInstance();
      $params = array(
        'id' => $contact_id,
      );
      $get_contact_results = $spcivi->api('Contact', 'get', $params);
      $debug_info = 'Getting contact to send mail. Submission id: ' . $submission->sid;
      if (_spcivipush_check_result($get_contact_results, 'contacten', 'get', $params, $debug_info)) {
        if (!empty($get_contact_results['values'])) {
          $contact_data = array_pop($get_contact_results['values']); 

          // Send fixed mail.
          if (!empty($sync_data['fixed_mail_text']['value'])) {
            $text_raw = $sync_data['fixed_mail_text']['value'];
            $filter_format = $sync_data['fixed_mail_text']['format'];
            $text = check_markup($text_raw, $filter_format);
            $subject = $sync_data['fixed_mail_subject'];
            spwebformsync_send_mail($contact_data, $subject, $text);
          }

          // Send variable mail.
          if (!empty($sync_data['variable_mail'])) {
            foreach ($sync_data['variable_mail'] as $component_id => $to_mail_data) {
              if (!empty($submission->data[$component_id])) {
                foreach ($submission->data[$component_id] as $answer_key) {
                  if (!empty($to_mail_data['values'][$answer_key]['mail']['value'])) {
                    $text_raw = $to_mail_data['values'][$answer_key]['mail']['value'];
                    $filter_format = $to_mail_data['values'][$answer_key]['mail']['format'];
                    $text = check_markup($text_raw, $filter_format);
                    $subject = $to_mail_data['values'][$answer_key]['subject'];
                    spwebformsync_send_mail($contact_data, $subject, $text);
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}

function spwebformsync_send_mail($contact_data, $subject, $text) {
  $site_mail = variable_get('site_mail', ini_get('sendmail_from'));
  if (!empty($site_mail)) {
    // Replace contact data in subject and body text.
    $raw_texts = array(
      'subject' => $subject,
      'text' => $text,
    );
    foreach ($raw_texts as $text_key => $raw_text) {
      $matches = array();
      // Replace contact data in raw text.
      if (preg_match_all('/\[([^]\s]+)\]/', $raw_text, $matches)) {
        foreach ($matches[1] as $key => $pattern_match) {
          if (!empty($contact_data[$matches[1][$key]])) {
            $raw_text = str_replace($matches[0][$key], check_plain($contact_data[$matches[1][$key]]), $raw_text);
          }
        }
        $$text_key = $raw_text;
      }
    }
    // Send mail
    $params = array(
      'text' => $text,
      'subject' => $subject,
    );
    $message = drupal_mail('spwebformsync', 'sync_mail', $contact_data['email'], language_default(), $params, $site_mail);
    // Process result.
    if (!empty($message['result'])) {
      // Mail verzonden.
      drupal_set_message('Er is een e-mail verzonden');
    }
    else {
      // Mail faal.
      drupal_set_message('Er is iets misgegaan met het verzenden van de e-mail.');
    }
  }
  else {
    drupal_set_message('Site e-mailadres is niet ingesteld. Neem contact op met webmaster@sp.nl.', 'error');
  }
}

function spwebformsync_push_contact_process_result($result, $submission, $sync_data) {
  $error = FALSE;
  if ($result === FALSE) {
    spwebformsync_store_submission_sync_state($submission->nid, $submission->sid, 'error');
    $error = TRUE;
  }
  elseif ($result === 'insufficient data') {
    spwebformsync_store_submission_sync_state($submission->nid, $submission->sid, 'insufficient data');
    $error = TRUE;
  }
  elseif ($result === 'contact id not found') {
    spwebformsync_store_submission_sync_state($submission->nid, $submission->sid, 'contact id not found');
    $error = TRUE;
  }
  return $error;
}
