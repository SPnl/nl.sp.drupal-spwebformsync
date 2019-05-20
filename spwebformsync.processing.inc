<?php

/* ---------------------------------------------------------------------------
 * SUBMISSION PROCESSING RELATED FUNCTIONS
 * ---------------------------------------------------------------------------
 */

function spwebformsync_process_submission($submission) {
  // Get target for sync data (sp datadrop server or civicrm).
  $target = variable_get('spwebformsync_sync_target', 'civicrm');

  $sync_data_all = variable_get('spwebformsync_sync_data', array());
  // Check if there is a spwebformsync for this webform.
  if (isset($sync_data_all[$submission->nid])) {
    watchdog('spwebformsync', 'Processing submission nr. ' . $submission->sid);
    $sync_data = $sync_data_all[$submission->nid];

    // Get source.
    // Create contact source line.
    $webform = node_load($sync_data['webform']);
    // Get link secret from component.
    $link_secret_component = spwebformsync_get_cid_by_form_key($webform, 'sp_unsubscribe_link_secret');
    $link_secret = '';
    if ($link_secret_component !== FALSE) {
      if (!empty($submission->data[$link_secret_component])) {
        $link_secret = $submission->data[$link_secret_component][0];
      }
    }
    // Get source url from component.
    $source_url_component = spwebformsync_get_cid_by_form_key($webform, 'source_url');
    $source_url = '';
    if ($source_url_component !== FALSE) {
      if (!empty($submission->data[$source_url_component])) {
        $source_url = ', url: ' . $submission->data[$source_url_component][0];
      }
    }
    $site_name = variable_get('site_name');
    global $base_url;
    $parsed_webform_url = parse_url($base_url);
    $source = 'SP webformsync: ' . $webform->title . ' (domain: ' . $parsed_webform_url['host'] . ', id: ' . $webform->nid . ', sid: ' . $submission->sid . $source_url . ')';
    $source_data = array(
      'source_domain' => $parsed_webform_url['host'],
      'source_title' => $webform->title,
      'source_id' => $webform->nid,
      'source_ip' => $submission->remote_addr,
      'submission_id' => $submission->sid,
      'secret' => $link_secret,
    );

    // Process spreferal.
    if (!empty($sync_data['sprefer_field']) && !empty($sync_data['sprefer_group'])) {
      $error = FALSE;
      // Process SP Referal.
      $referal_id = $submission->data[$sync_data['sprefer_field']][0];
      $data = array(
        'contact_id' => $referal_id,
        'source' => $source,
      );
      $groups = array($sync_data['sprefer_group'] => $sync_data['sprefer_group']);
      $data['groups'] = $groups;

      switch ($target) {
        case 'civicrm':
        $result = spcivipush_push_contact($data, $submission);
        break;

        case 'datadrop':
        $result = spwebformsync_datadrop($data, $submission, $source_data);
        break;
      }
      if ($result === FALSE) {
        spwebformsync_store_submission_sync_state($submission->nid, $submission->sid, 'error');
        watchdog('spwebformsync', 'Processed submission nr. ' . $submission->sid);
        return;
      }
      elseif ($result === 'contact id not found') {
        spwebformsync_store_submission_sync_state($submission->nid, $submission->sid, 'contact id not found');
        watchdog('spwebformsync', 'Processed submission nr. ' . $submission->sid);
        return;
      }
    }

    // Process contact data.
    // Parse autosync data.
    $data = spwebformsync_parse_submission($submission, $sync_data);

    // Parse other data.
    $data['overwrite'] = empty($sync_data['overwrite']['overwrite']) ? FALSE : TRUE;
    $data['source'] = $source;
    $data['author'] = $submission->name;
    $data['remote_addr'] = $submission->remote_addr;

    // Notes.
    if (!empty($sync_data['notes'])) {
      foreach ($sync_data['notes'] as $key => $note_data) {
        $note_content = '';
        $note_use = array();
        if (count($note_data['note_fields']) > 1) {
          foreach ($note_data['note_fields'] as $component_id) {
            if (!empty($submission->data[$component_id])) {
              $submitted_data = $submission->data[$component_id];
              if (!empty($submitted_data)) {
                $field_content = check_markup(reset($submitted_data), 'filtered_html');
                if (!empty($field_content)) {
                  $note_content .= '<div class="note_content">' . $field_content . '</div>';
                }
              }
            }
          }
        }
        else {
          $component_id = array_pop($note_data['note_fields']);
          if (!empty($submission->data[$component_id])) {
            $submitted_data = $submission->data[$component_id];
            if (!empty($submitted_data)) {
              $field_content = check_markup(reset($submitted_data), 'filtered_html');
              if (!empty($field_content)) {
                $note_content = $field_content;
              }
            }
          }
        }
        if (!empty($note_content)) {
          $data['notes'][$key]['note_content'] = $note_content;
          $data['notes'][$key]['note_use'] = empty($note_data['note_use']) ? array() : $note_data['note_use'];
        }
      }
    }

    // Parse groups.
    $groups = spwebformsync_parse_groups($submission, $sync_data);
    $data['groups'] = $groups;

    // Add sp_active activity data if available.
    $sp_active = spwebformsync_parse_activities($webform, $submission);
    if (!empty($sp_active)) {
      $data['sp_active'] = $sp_active;
    }

    // Sync submission to CiviCRM.
    switch ($target) {
      case 'civicrm':
        $result = spcivipush_push_contact($data, $submission);
        break;

      case 'datadrop':
        $result = spwebformsync_datadrop($data, $submission, $source_data);
        break;
    }
    spwebformsync_push_contact_process_result($result, $submission, $sync_data, $webform);
  }
}

/**
 * Parses submission data with sync info in raw data array to sync.
 */
function spwebformsync_parse_submission($submission, $sync_data) {
  $data = array();
  $sync_items = spwebformsync_civi_fields();
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
          spwebformsync_process_submission($submission);
        }
        else {
          spwebformsync_store_submission_sync_state($webform_id, $submission_id, 'insufficient data');
          watchdog('spwebformsync', 'Processed submission nr. ' . $submission->sid);
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

function spwebformsync_send_mails($contact_id, $sync_data, $submission, $webform) {
  $allowed_types = variable_get('spwebformsync_allowed_component_types', array());
  $webform_components = spwebformsync_get_webform_allowed_components($webform->nid, $allowed_types);
  if (!empty($sync_data)) {
    if (
      !empty($sync_data['fixed_mail_text']['value']) ||
      !empty($sync_data['afdeling_mail']) ||
      !empty($sync_data['variable_mail'])
    ) {
      // Get contact data from CiviCRM.
      $spcivi = \SPCivi::getInstance();
      $params = array(
        'contact_id' => $contact_id,
        'include_non_menmbers' => 1,
      );
      $get_contact_results = $spcivi->api('Contact', 'getspdata', $params);
      $debug_info = 'Getting contact to send mail. Submission id: ' . $submission->sid;
      if (_spcivipush_check_result($get_contact_results, 'contacten', 'get', $params, $debug_info)) {
        if (!empty($get_contact_results['values'])) {
          $contact_data = array_pop($get_contact_results['values']);

          if (!empty($contact_data['email'])) {
            // Send fixed mail.
            if (!empty($sync_data['fixed_mail_text']['value'])) {
              $subject = '';
              if (empty($contact_data['email'])) {
                $mailto = variable_get('site_mail', ini_get('sendmail_from'));
                $subject = 'Niet aan contact verzonden (e-mailadres contact onbekend): ';
              }
              else {
                $mailto = $contact_data['email'];
              }
              if (!empty($mailto)) {
                $text_raw = $sync_data['fixed_mail_text']['value'];
                $filter_format = $sync_data['fixed_mail_text']['format'];
                $text = check_markup($text_raw, $filter_format);
                $subject .= $sync_data['fixed_mail_subject'];
                $mailfrom = $sync_data['fixed_mail_sender_emailadress'];
                spwebformsync_send_mail($contact_data, $submission, $webform, $subject, $text, $mailto, $mailfrom);
              }
            }
          }

          // Send variable mail and afdeling mail.
          $types = array(
            'variable',
            'afdeling',
          );
          foreach ($types as $type) {
            if (!empty($sync_data[$type . '_mail'])) {
              $subject = '';
              if ($type === 'variable') {
                // Variable mail.
                if (empty($contact_data['email'])) {
                  $mailto = variable_get('site_mail', ini_get('sendmail_from'));
                  $subject = 'Niet aan contact verzonden (e-mailadres contact onbekend): ';
                }
                else {
                  $mailto = $contact_data['email'];
                }
              }
              else {
                // Afdeling mail.
                if (empty($contact_data['afdeling_code'])) {
                  $mailto = variable_get('site_mail', ini_get('sendmail_from'));
                  $subject = 'Niet aan afdeling verzonden (afdeling van contact onbekend): ';
                }
                else {
                  $afdeling = spwebformsync_get_afdeling($contact_data['afdeling_code']);
                  $afdeling_wrapper = entity_metadata_wrapper('node', $afdeling);
                  if (!isset($afdeling_wrapper->afd_email) || empty($afdeling_wrapper->afd_email->value())) {
                    $mailto = variable_get('site_mail', ini_get('sendmail_from'));
                    $subject = 'Niet aan afdeling verzonden (e-mailadres afdeling onbekend): ';
                  }
                  else {
                    $mailto = $afdeling_wrapper->afd_email->value();
                  }
                }
              }
              if (empty($mailto)) continue;
              foreach ($sync_data[$type . '_mail'] as $component_id => $to_mail_data) {
                if (!empty($submission->data[$component_id])) {
                  foreach ($submission->data[$component_id] as $answer_key) {
                    if (!empty($to_mail_data['values'][$answer_key]['mail']['value'])) {
                      $data = $to_mail_data['values'][$answer_key];
                      $text_raw = $data['mail']['value'];
                      $filter_format = $data['mail']['format'];
                      $text = check_markup($text_raw, $filter_format);
                      $subject .= $data['subject'];
                      $mailfrom = $data['sender'];
                      spwebformsync_send_mail($contact_data, $submission, $webform, $subject, $text, $mailto, $mailfrom);
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
}

function spwebformsync_replace_tokens($contact_data, $submission, $webform, $raw_text) {
  $matches = array();
  $text = $raw_text;
  $token_components = spwebformsync_filter_token_components($webform->webform['components']);
  if (preg_match_all('/\[([^]\s]+)\]/', $raw_text, $matches)) {
    foreach ($matches[1] as $key => $pattern_match) {
      // Replace contact data in raw text.
      if (!empty($contact_data[$matches[1][$key]])) {
        $text = str_replace($matches[0][$key], check_plain($contact_data[$matches[1][$key]]), $text);
      }
      // Replace component tokens in raw text.
      if (!empty($token_components)) {
        foreach ($token_components as $token_component) {
          $items_parsed = array();
          if ($matches[1][$key] == $token_component['form_key']) {
            $mapping = spwebformsync_parse_webform_select_options($webform, $token_component['cid']);
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
  return $text;
}

function spwebformsync_send_mail($contact_data, $submission, $webform, $subject, $text, $mailto, $mailfrom) {
  $site_mail = variable_get('site_mail', ini_get('sendmail_from'));
  if (empty($mailfrom)) $mailfrom = $site_mail;
  if (!empty($mailfrom)) {
    // Replace contact data in subject and body text.
    $raw_texts = array(
      'subject' => $subject,
      'text' => $text,
    );
    foreach ($raw_texts as $text_key => $raw_text) {
      $$text_key = spwebformsync_replace_tokens($contact_data, $submission, $webform, $raw_text);
    }
    // Send mail
    $params = array(
      'subject' => $subject,
      'text' => $text,
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
  else {
    $contact_id = $result;
    spwebformsync_store_submission_sync_state($submission->nid, $submission->sid, 'synced');
    spwebformsync_send_mails($contact_id, $sync_data, $submission, $webform);
  }
  watchdog('spwebformsync', 'Processed submission nr. '.$submission->sid);
  return $error;
}

function spwebformsync_get_afdeling($afdeling_id) {
  $query = new EntityFieldQuery();
  $result= $query->entityCondition('entity_type', 'node')
    ->entityCondition('bundle', 'afdeling')
    ->propertyCondition('status', NODE_PUBLISHED)
    ->fieldCondition('afd_id', 'value', $afdeling_id, '=')
    ->fieldCondition('afd_status', 'value', 'Erkend', '=')
    ->execute();

  if (isset($result['node'])) {
    $afdeling_items_nids = array_keys($result['node']);
    $afdeling_items = entity_load('node', $afdeling_items_nids);
    if (!empty($afdeling_items)) {
      return array_pop($afdeling_items);
    }
  }
}

function spwebformsync_civi_fields() {
  $data_names = array(
    'contact_id' => 'Contact id',
    'name' => 'gehele naam',
    'first_name' => 'voornaam',
    'middle_name' => 'tussenvoegsel',
    'last_name' => 'achternaam',
    'email' => 'e-mail',
    'telephone' => 'telefoon',
    'street' => 'straat',
    'house_number' => 'huisnummer (inc. toevoeging)',
    'house_number_addition' => 'huisnummer toevoeging',
    'street_and_number' => 'straat en huisnummer',
    'postal_code' => 'postcode',
    'locality' => 'plaats',
  );
  return $data_names;
}

/**
 * Push contact to datadrop.
 *
 * Data: array()
 *   keys:
 *     - contact_id
 *     - name
 *     - first_name
 *     - middle_name
 *     - last_name
 *     - initials
 *     - birth_date (1974-01-28)
 *     - gender (m,v,a)
 *     - email
 *     - telephone
 *     - phone_mobile
 *     - street
 *     - house_number
 *     - house_number_addition
 *     - street_and_number
 *     - postal_code
 *     - locality
 *     - country (NL)
 *     - notes: array(
 *         array(
 *           note_content,
 *           note_use: array(
 *             intern
 *           )
 *         )
 *         array(
 *           note_content
 *           note_use: array(
 *           )
 *         )
 *         ...
 *       )
 *     - overwrite
 *     - author
 *     - source
 *     - groups: array(group_id_1, group_id_2, etc.)
 *     - sp_active: array(
 *         active,
 *         activities: array(
 *         )
 *       )
 *
 *
 * debug_info: variable content shown in logs.
 */
function spwebformsync_datadrop($data, $debug_info, $source_data) {
  if (
    // Check if testing.
    (!empty($data['name']) && strpos(strtolower($data['name']), '-test') !== FALSE) ||
    (!empty($data['first_name']) && strpos(strtolower($data['first_name']), '-test') !== FALSE) ||
    (!empty($data['last_name']) && strpos(strtolower($data['last_name']), '-test') !== FALSE)
  ) {
    drupal_set_message('Test data, not sending to CiviCRM');
    return 'insufficient data';
  }

  // Send data to datadrop server.
  $url = variable_get('spwebformsync_datadrop_server') . '/spdatadrop';
  $secret = variable_get('spwebformsync_datadrop_secret');
  $data_to_send = array(
    'submission_data' => $data,
    'source_data' => $source_data,
    'debug_info' => $debug_info,
    'secret' => $secret,
  );
  $request_body = http_build_query($data_to_send);
  $headers = array('Content-Type' => 'application/x-www-form-urlencoded');
  $options = array(
    'method' => 'POST',
    'data' => $request_body,
    'headers' => $headers,
  );
  $result = drupal_http_request($url, $options);
  switch ($result->code) {
    case '200':
      watchdog('spwebformsync', 'Pushed data to spdatadrop location');
      break;

    default:
      $message_data = array(
        'url' => $url,
        'headers' => $headers,
        'post data' => $options,
        'result' => $result,
      );
      watchdog('spwebformsync', 'Error pushing data to spdatadrop location: <pre>@message_data</pre>', array('@message_data' => print_r($message_data, TRUE)), WATCHDOG_ERROR);
      return FALSE;
  }
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
            if (!empty($first_element)) {
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
