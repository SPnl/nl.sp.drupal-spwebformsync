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

    // Get debug info.
    $webform = node_load($sync_data['webform']);
    $site_name = variable_get('site_name');
    global $base_url;
    $parsed_webform_url = parse_url($base_url);
    $debug_info = array(
      'source_domain' => $parsed_webform_url['host'],
      'source_title' => $webform->title,
      'source_id' => $webform->nid,
      'source_ip' => $submission->remote_addr,
      'submission_id' => $submission->sid,
      'remote_addr' => $submission->remote_addr,
    );

    // Process contact data.
    // Parse autosync data.
    $data = spwebformsync_parse_submission($submission, $sync_data);

    // Parse other data.
    $data['overwrite'] = empty($sync_data['overwrite']['overwrite']) ? FALSE : TRUE;

    // Parse groups.
    $groups = spwebformsync_parse_groups($submission, $sync_data);
    if (!empty($groups)) {
      $data['selections']['add'] = $groups;
    }

    // Parse rgroups.
    $groups = spwebformsync_parse_rgroups($submission, $sync_data);
    if (!empty($groups)) {
      $data['selections']['remove'] = $groups;
    }

    // Parse sp news.
    $sp_news_subscription = spwebformsync_parse_sp_news_subscription($submission, $sync_data);
    if ($sp_news_subscription) {
      $data['sp_news_subscription'] = TRUE;
    }

    if (module_exists('sp_active')) {
      // Add sp_active activity data if available.
      $sp_active = spwebformsync_parse_activities($webform, $submission);
      if (!empty($sp_active)) {
        $data['sp_active'] = $sp_active;
      }
    }

    if (module_exists('sp_work_and_int')) {
      // Add work and interesests data if available.
      $sp_work_and_int = spwebformsync_parse_work_and_interests($webform, $submission);
      if (!empty($sp_work_and_int)) {
        $data['sp_work_and_int'] = $sp_work_and_int;
      }
    }

    if (module_exists('sp_privacy')) {
      // Add privacy data if available.
      $sp_privacy = spwebformsync_parse_privacy($webform, $submission);
      if (!empty($sp_privacy)) {
        $data['privacy'] = $sp_privacy;
      }
    }

    // Temporary filter out sumbission data without e-mail.
    if (empty($data['email'])) {
      $filter_submissions_without_email = variable_get('spwebformsync_filter_submissions_without_email');
      if (!empty($filter_submissions_without_email)) {
        spwebformsync_store_submission_sync_state($submission->nid, $submission->sid, 'no email');
        watchdog('spwebformsync', 'Stopped processing submission nr. ' . $submission->sid . ', temporary filter out submissions without e-mail.');
        return;
      }
    }

    // Filter out submissions from beltool that are not status answered_completed.
    if (module_exists('spbellen')) {
      if (!empty($submission)) {
        if (spbellen_is_spbellen_form($submission->nid)) {
          // Get action.
          $sql = "SELECT result FROM {spbellen_action} WHERE webform_submission_id = :sid";
          $result = db_query($sql, array(':sid' => $submission->sid));
          $status = $result->fetchField();
          if ($status !== 'answered_completed') {
            spwebformsync_store_submission_sync_state($submission->nid, $submission->sid, 'beltool not answered completed');
            watchdog('spwebformsync', 'Stopped processing submission nr. ' . $submission->sid . ', filter out beltool submission unfinished call (' . $status . ')');
            return;
          }
        }
      }
    }

    // Filter out sumbission data without e-mail.
    if (empty($data['selections']) && empty($data['sp_news_subscription']) && empty($data['privacy'])) {
      $filter_submissions_without_selection = variable_get('spwebformsync_filter_submissions_without_selection');
      if (!empty($filter_submissions_without_selection)) {
        spwebformsync_store_submission_sync_state($submission->nid, $submission->sid, 'no selection');
        watchdog('spwebformsync', 'Stopped processing submission nr. ' . $submission->sid . ', filter out submissions without selection or newssubscription.');
        return;
      }
    }

    // Sync submission to Procurios.
    $result = procpush_push_contact($data, $debug_info);
    spwebformsync_push_contact_process_result($result, $submission, $sync_data, $webform);
  }
}

/**
 * Parses submission data with sync info in raw data array to sync.
 */
function spwebformsync_parse_submission($submission, $sync_data) {
  $data = array();
  $sync_items = spwebformsync_contact_data_fields();
  foreach ($sync_items as $key => $name) {
    $var = '';
    if (!empty($sync_data[$key . '_component'])) {
      $component_id = $sync_data[$key . '_component'];
      $parts = explode('|', $component_id);
      if (isset($parts[1])) {
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

/**
 * Parses submission data with sync info in raw data array to sync.
 */
function spwebformsync_parse_rgroups($submission, $sync_data) {
  $groups = array();
  // Get variable groups.
  $submission_data = $submission->data;
  if (!empty($sync_data['rselected_variable_groups'])) {
    foreach ($sync_data['rselected_variable_groups'] as $sync_values) {
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

/**
 * Parse spnew submission data.
 */
function spwebformsync_parse_sp_news_subscription($submission, $sync_data) {
  $submission_data = $submission->data;
  // Get sp news group.
  if (!empty($sync_data['sp_news_subscription_component']) && !empty($sync_data['sp_news_subscription_option'])) {
    if (in_array($sync_data['sp_news_subscription_option'], $submission_data[$sync_data['sp_news_subscription_component']])) {
      return TRUE;
    }
  }
  return FALSE;
}

function _spcf_check_result($result, $dataname, $action, $data = NULL, $submission = NULL) {
  $action_name = ($action === 'get') ? 'ophalen' : 'aanmaken';
  $place_name = ($action === 'get') ? 'uit' : 'in';
  if (!isset($result['is_error']) || $result['is_error'] == 1) {
    if (empty($submission)) {
      drupal_set_message('Fout bij het ' . $action_name . ' van ' . $dataname . '.', 'error');
      watchdog('spwebformsync', '<p>Fout bij het ' . $action_name . ' van ' . $dataname . '.<pre>' . print_r($result, TRUE) . '</pre>Parameters:<pre>' . print_r($data, TRUE) . '</pre>', array(), WATCHDOG_ERROR);
    }
    else {
      drupal_set_message('Fout bij het ' . $action_name . ' van ' . $dataname . ' ' . $place_name . ' CRM (sid = ' . $submission->sid . ').', 'error');
      watchdog('spwebformsync', '<p>Fout bij het ' . $action_name . ' van ' . $dataname . ' ' . $place_name . ' CRM (sid = ' . $submission->sid . ').</p>Resultaat:<pre>' . print_r($result, TRUE) . '</pre>Parameters:<pre>' . print_r($data, TRUE) . '</pre>', array(), WATCHDOG_ERROR);
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
  $states = spwebformsync_get_states();
  if (!empty($nid) && !empty($sid) && isset($states[$state])) {
    $new_state = $states[$state];
    // Check if state exists.
    $query = "SELECT state FROM {spwebformsync_syncstate} WHERE nid = :nid AND sid = :sid";
    $result = db_query($query, array(':nid' => $nid, ':sid' => $sid));
    $old_state = $result->fetchField(0);
    if ($old_state !== FALSE) {
      if ($old_state == 2 && $new_state == 2) {
        $new_state = 9;
      }
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
    $check_confirmation = FALSE;
    $sync_data = variable_get('spwebformsync_sync_data', array());
    if (!empty($sync_data[$nid]['confirmation_check'])) {
      if (!empty($sync_data[$nid]['confirmation_check']['confirmation_check'])) {
        $check_confirmation = TRUE;
        watchdog('spwebformsync', 'Check confirmation for ' . $nid);
      }
    }

    // Create query.
    $query = db_select('spwebformsync_syncstate', 'state');
    if ($check_confirmation) {
      $query->join('webform_submissions', 'sub', 'state.sid = sub.sid');
      $result = $query
        ->fields('state', array('sid'))
        ->condition('sub.confirmed', 1)
        ->condition('state.state', 0)
        ->condition('state.nid', $nid)
        ->range(0,1)
        ->execute();
    }
    else {
      $result = $query
        ->fields('state', array('sid'))
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
    watchdog('spwebformsync', 'Number of webforms to sync: ' . $number_of_webforms_to_sync);
    $count = 0;
    // Check max number of unprocessed submissions.
    $states = spwebformsync_get_states();
    $query = "SELECT COUNT(*) FROM {spwebformsync_syncstate} WHERE state = :state";
    $result = db_query($query, array('state' => $states['processing']));
    $unprocessed = $result->fetchField();
    if ($unprocessed > 0) {
      watchdog('spwebformsync', 'Skipping sync, previous syncs still in progress');
      return $processed;
    }
    // Sync.
    while ($count < $max_cron_number && $items_found) {
      $items_found = FALSE;
      foreach ($sync_data as $webform_id => $data) {
        $submission_id = spwebformsync_get_unprocessed_submission_id($webform_id);
        if (empty($submission_id)) {
          continue;
        }
        $submission = webform_get_submission($webform_id, $submission_id);
        if (!empty($submission)) {
          spwebformsync_store_submission_sync_state($webform_id, $submission_id, 'processing');
          spwebformsync_process_submission($submission);
        }
        else {
          spwebformsync_store_submission_sync_state($webform_id, $submission_id, 'insufficient data');
          watchdog('spwebformsync', 'Submission nr. ' . $submission_id . ' is empty, setting state to insufficient data.');
        }
        $items_found = TRUE;
        $count ++;
        $processed[$submission_id] = $submission_id;
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

function spwebformsync_send_mails($sync_data, $submission, $webform) {
  $allowed_types = variable_get('spwebformsync_allowed_component_types', array());
  if (module_exists('spcontact')) {
    $allowed_types['spcontact'] = 'spcontact';
  }
  if (!empty($sync_data)) {
    if (!empty($sync_data['fixed_mail_text']['value'])) {
      // Send fixed mail.
      $subject = '';
      $mail_to_component = $sync_data['fixed_mail_to_component'];
      $parts = explode('|', $mail_to_component);
      $mailto = '';
      if (isset($parts[1])) {
        if (!empty($submission->data[$parts[0]][$parts[1]])) {
          $mailto = $submission->data[$parts[0]][$parts[1]];
        }
      }
      else {
        if (!empty($submission->data[$mail_to_component][0])) {
          $mailto = $submission->data[$mail_to_component][0];
        }
      }
      if (!empty($mailto)) {
        $text_raw = $sync_data['fixed_mail_text']['value'];
        $filter_format = $sync_data['fixed_mail_text']['format'];
        $text = check_markup($text_raw, $filter_format);
        $subject .= $sync_data['fixed_mail_subject'];
        $mailfrom = $sync_data['fixed_mail_sender_emailadress'];
        spwebformsync_send_mail($submission, $webform, $subject, $text, $mailto, $mailfrom);
      }
    }

    if (!empty($sync_data['variable_mail'])) {
      // Send variable mail.
      $subject = '';
      // Variable mail.
      $mail_to_component = $sync_data['variable_mail_to_component'];
      $parts = explode('|', $mail_to_component);
      $mailto = '';
      if (isset($parts[1])) {
        if (!empty($submission->data[$parts[0]][$parts[1]])) {
          $mailto = $submission->data[$parts[0]][$parts[1]];
        }
      }
      else {
        if (!empty($submission->data[$mail_to_component])) {
          if (!empty($submission->data[$mail_to_component][0])) {
            $mailto = $submission->data[$mail_to_component][0];
          }
        }
      }
      if (!empty($mailto)) {
        foreach ($sync_data['variable_mail'] as $component_id => $to_mail_data) {
          if (!empty($submission->data[$component_id])) {
            foreach ($submission->data[$component_id] as $answer_key) {
              if (!empty($to_mail_data['values'][$answer_key]['mail']['value'])) {
                $data = $to_mail_data['values'][$answer_key];
                $text_raw = $data['mail']['value'];
                $filter_format = $data['mail']['format'];
                $text = check_markup($text_raw, $filter_format);
                $subject .= $data['subject'];
                $mailfrom = $data['sender'];
                spwebformsync_send_mail($submission, $webform, $subject, $text, $mailto, $mailfrom);
              }
            }
          }
        }
      }
    }
  }
}

function spwebformsync_replace_tokens($submission, $webform, $raw_text) {
  $matches = array();
  $text = $raw_text;
  $token_components = spwebformsync_filter_token_components($webform->webform['components']);
  if (preg_match_all('/\[([^]\s]+)\]/', $raw_text, $matches)) {
    foreach ($matches[1] as $key => $pattern_match) {
      // Replace component tokens in raw text.
      if (!empty($token_components)) {
        foreach ($token_components as $token_component) {
          $items_parsed = array();
          if ($matches[1][$key] == $token_component['form_key']) {
            $mapping = spwebformsync_parse_webform_select_options($webform->nid, $token_component['cid']);
            if (!empty($submission->data[$token_component['cid']])) {
              $items = $submission->data[$token_component['cid']];
              foreach ($items as $item) {
                $item_parsed = isset($mapping[$item]) ? $mapping[$item] : $item;
                $items_parsed[] = $item_parsed;
              }
              $line = join(' en ', array_filter(array_merge(array(join(', ', array_slice($items_parsed, 0, -1))), array_slice($items_parsed, -1)), 'strlen'));
              $text = str_replace($matches[0][$key], check_plain($line), $text);
            }
          }
        }
      }

    }
  }
  return $text;
}

function spwebformsync_send_mail($submission, $webform, $subject, $text, $mailto, $mailfrom) {
  $site_mail = variable_get('site_mail', ini_get('sendmail_from'));
  if (empty($mailfrom)) {
    $mailfrom = $site_mail;
  }
  if (!empty($mailfrom)) {
    // Replace tokens in subject and body text.
    $raw_texts = array(
      'subject' => $subject,
      'text' => $text,
    );
    foreach ($raw_texts as $text_key => $raw_text) {
      $$text_key = spwebformsync_replace_tokens($submission, $webform, $raw_text);
    }
    // Send mail
    $params = array(
      'subject' => $subject,
      'text' => $text,
      'node' => $webform,
      'submission' => $submission,
    );
    $message = drupal_mail('spwebformsync', 'sync_mail', $mailto, language_default(), $params, $mailfrom);
    // Process result.
    if (!empty($message['result'])) {
      // Mail verzonden.
      drupal_set_message('Er is een e-mail verzonden');
      watchdog('spwebformsync', 'Er is een e-mail verzonden aan ' . check_plain($mailto) .  '.');
    }
    else {
      // Mail faal.
      drupal_set_message('Er is iets misgegaan met het verzenden van de e-mail aan ' . check_plain($mailto) .  '.');
      watchdog('spwebformsync', 'Er is iets misgegaan bij het verzenden van een e-mail aan ' . check_plain($mailto) .  '.', WATCHDOG_ERROR);
    }
  }
  else {
    drupal_set_message('Site e-mailadres is niet ingesteld. Neem contact op met webmaster@sp.nl.', 'error');
  }
}

function spwebformsync_push_contact_process_result($result, $submission, $sync_data, $webform) {
  $error = FALSE;
  if ($result) {
    spwebformsync_store_submission_sync_state($submission->nid, $submission->sid, 'synced');
    spwebformsync_send_mails($sync_data, $submission, $webform);
    if ($result == 'insufficient_data') {
      watchdog('spwebformsync', 'Insufficient data to push submission ' . $submission->sid . ' to Procurios.');
    }
    else {
      watchdog('spwebformsync', 'Succesfully pushed submission (' . $submission->sid . ') to Procurios (' . $result . ').');
    }
  }
  else {
    spwebformsync_store_submission_sync_state($submission->nid, $submission->sid, 'error');
    watchdog('spwebformsync', 'Error pushing submission (' . $submission->sid . ') to Procurios.');
  }
  watchdog('spwebformsync', 'Processed submission nr. '.$submission->sid);
}

function spwebformsync_contact_data_fields() {
  $data_fields = procpush_data_fields_flat_list();
  return $data_fields;
}


/**
 * Parses activity data from submission for sending to spcivipush module.
 */
function spwebformsync_parse_activities($webform, $submission) {
  $parsed_data = array();
  if (isset($submission->data)) {
    if (!empty($webform->webform['components'])) {
      foreach ($webform->webform['components'] as $key => $component) {
        if (!empty($component['type'] && $component['type'] === 'sp_active')) {
          if (!empty($submission->data[$key])) {
            // Parse active.
            $data_to_parse = $submission->data[$key];
            $first_element = array_shift($data_to_parse);
            if (isset($first_element)) {
              $parsed_data['active'] = empty($first_element) ? '' : $first_element;
              $parsed_data['activities'] = array();
              foreach ($data_to_parse as $activity) {
                if (!empty($activity)) {
                  $parsed_data['activities'][$activity] = $activity;
                }
              }
              break;
            }
          }
        }
      }
    }
  }
  return $parsed_data;
}

/**
 * Parses work and interests component data from submission for sending to spcivipush module.
 */
function spwebformsync_parse_work_and_interests($webform, $submission) {
  $parsed_data = array();
  if (isset($submission->data)) {
    if (!empty($webform->webform['components'])) {
      foreach ($webform->webform['components'] as $key => $component) {
        if (!empty($component['type'] && $component['type'] === 'sp_work_and_int')) {
          if (!empty($submission->data[$key])) {
            $option_groups = sp_work_and_int_get_option_groups();
            // Parse active.
            $data_to_parse = $submission->data[$key];
            $no = 0;
            foreach ($option_groups as $option_group) {
              $value = $data_to_parse[$no];
              if ($option_group['type'] == 'radios') {
                if (!empty($value)) {
                  $parsed_data[$option_group['key']] = $value;
                }
                $no++;
              }
              else {
                foreach ($option_group['options'] as $option) {
                  if (!empty($value)) {
                    $parsed_data[$option_group['key']][$value] = $value;
                  }
                  $no++;
                }
              }
            }
          }
        }
      }
    }
  }
  return $parsed_data;
}

/**
 * Parses privacy data from submission for sending to spcivipush module.
 */
function spwebformsync_parse_privacy($webform, $submission) {
  $parsed_data = array();
  if (isset($submission->data)) {
    if (!empty($webform->webform['components'])) {
      foreach ($webform->webform['components'] as $key => $component) {
        if (!empty($component['type'] && $component['type'] === 'sp_privacy')) {
          if (!empty($submission->data[$key])) {
            $data_to_parse = $submission->data[$key];
            $parsed_data = array();
            foreach ($data_to_parse as $key => $value) {
              $parsed_data[$key] = empty($value) ? FALSE : TRUE;
            }
            break;
          }
        }
      }
    }
  }
  return $parsed_data;
}

/**
 * Given determine cid of source url component.
 *
 * @param $node
 *   A fully loaded node object.
 * @return array
 */
function spwebformsync_get_cid_by_form_key($node, $form_key) {
  $cids = array();
  foreach ($node->webform['components'] as $cid => $component) {
    if ($component['form_key'] == $form_key) {
      return $cid;
    }
  }
}

/**
 * Return sync states.
 */
function spwebformsync_get_states() {
  $states = array(
    'unprocessed' => '0',
    'synced' => '1',
    'error' => '2',
    'insufficient data' => '3',
    'contact id not found' => '4',
    'no email' => '5',
    'no selection' => '6',
    'processing' => '7',
    'beltool not answered completed' => '8',
    'error after second try' => '9',
  );
  return $states;
}
