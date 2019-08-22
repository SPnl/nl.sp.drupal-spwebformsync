<?php

function spwebformsync_admin_form($form, &$form_state, $form_id = NULL) {
  $values = empty($form_state['values']) ? array() : $form_state['values'];
  $sync_data = variable_get('spwebformsync_sync_data', array());
  if (!empty((int) $form_id)) {
    $webform_default = (int) $form_id;
    spwebformsync_store_sync_data($form_state, $webform_default);
  }
  else {
    $webform_default = isset($values['webform']) ? $values['webform'] : '';
  }

  $form = array();

  $form['#prefix'] = '<div id="spwebformsync_ajax_form">';
  $form['#suffix'] = '</div>';

  // Select webform element.
  // ----------------------------------------------------------

  // Get data.
  $webforms = spwebformsync_get_webforms();
  if (!empty($webforms)) {
    foreach ($webforms as $webform) {
      $webform_select_options[$webform->nid] = $webform->title . ' (' . $webform->nid . ')';
    }
    krsort($webform_select_options);

    // Create form element.
    $form['webform'] = array(
      '#type' => 'select',
      '#title' => 'Kies het webformulier',
      '#options' => $webform_select_options,
      '#empty_option' => 'Kies:',
      '#name' => 'webform',
      '#ajax' => array(
        'wrapper' => 'spwebformsync_ajax_form',
        'callback' => 'spwebformsync_ajax_form_callback',
      ),
    );
  }
  else {
    $form['no_webforms'] = array(
      '#markup' => 'Er is geen webformulier beschikbaar (<a href="/node/webform/add">voeg toe</a>).',
    );
  }

  if (!empty($webform_default)) {
    // Set default value.
    $form['webform']['#default_value'] = $webform_default;
    unset($form['webform']['#empty_option']);


    $allowed_types = variable_get('spwebformsync_allowed_component_types', array());

    $data_names = spwebformsync_civi_fields();
    $webform_components = spwebformsync_get_webform_allowed_components($webform_default, $allowed_types);
    if (!empty($webform_components)) {

      $webform = node_load($webform_default);
      $webform_component_select_options = spwebformsync_get_components_list($webform_components);

      // Add select field components.
      // ----------------------------------------------------------
      $form['fields'] = array(
        '#type' => 'fieldset',
        '#title' => 'Selecteer naar het CiviCRM contact te synchronizeren webformulier velden (optioneel)',
      );

      foreach ($data_names as $data_key => $data_name) {
        $default_value = '';
        if (!empty($sync_data[$webform->nid][$data_key . '_component'])) {
          $default_value = $sync_data[$webform->nid][$data_key . '_component'];
        }
        elseif (module_exists('spbellen') && spbellen_is_spbellen_form($webform->nid)) {
          // Set default sync fields for beltool.
          switch ($data_key) {
            case 'contact_id':
              $default_value = array_search('CRM contact id (verborgen)', $webform_component_select_options);
              break;

            case 'name':
              $default_value = array_search('Weergave naam', $webform_component_select_options);
              break;

            case 'email':
              $default_value = array_search('E-mail', $webform_component_select_options);
              break;

            case 'telephone':
              $default_value = array_search('Telefoonnummer', $webform_component_select_options);
              break;

            case 'street':
              $default_value = array_search('Adresinformatie: Straat', $webform_component_select_options);
              break;

            case 'house_number':
              $default_value = array_search('Adresinformatie: Huisnummer', $webform_component_select_options);
              break;

            case 'postal_code':
              $default_value = array_search('Adresinformatie: Postcode', $webform_component_select_options);
              break;

            case 'locality':
              $default_value = array_search('Adresinformatie: Plaats', $webform_component_select_options);
              break;
          }
        }
        // Select name component.
        $form['fields'][$data_key . '_component'] = array(
          '#type' => 'select',
          '#title' => ucfirst($data_name),
          '#options' => $webform_component_select_options,
          '#empty_option' => '- Geen -',
          '#default_value' => $default_value,
          '#ajax' => array(
            'wrapper' => 'spwebformsync_ajax_form',
            'callback' => 'spwebformsync_ajax_form_callback',
          ),
        );

        // Set default value.
        if (isset($values[$data_key . '_component'])) {
          $form['fields'][$data_key . '_component']['#default_value'] = $values[$data_key . '_component'];
        }
      }

      // Add option to overwrite existing civi data.
      if (!empty($sync_data[$webform->nid]['contact_id_component']) && !isset($values['contact_id_component'])) {
        $values['contact_id_component'] = $sync_data[$webform->nid]['contact_id_component'];
      }
      elseif (module_exists('spbellen') && spbellen_is_spbellen_form($webform->nid)) {
        // Beltool form, set overwrite by default.
        $values['contact_id_component'] = array_search('CRM contact id (verborgen)', $webform_component_select_options);
      }
      if (!empty($values['contact_id_component'])) {
        $default_value = array();
        if (isset($sync_data[$webform->nid]['overwrite']['overwrite'])) {
          if (empty($sync_data[$webform->nid]['overwrite']['overwrite'])) {
            $default_value = array();
          }
          else {
            $default_value = array('overwrite');
          }
        }
        elseif (module_exists('spbellen') && spbellen_is_spbellen_form($webform->nid)) {
          // Beltool form, set overwrite by default.
          $default_value = array('overwrite');
        }
        $form['fields']['overwrite'] = array(
          '#type' => 'checkboxes',
          '#title' => 'Overschrijf bestaande waarden in CiviCRM',
          '#default_value' => $default_value,
          '#options' => array('overwrite' => 'Overschrijf bestaande CiviCRM waarden'),
        );
        // Set default value.
        if (isset($values['overwrite'])) {
          $form['fields']['overwrite']['#default_value'] = $values['overwrite'];
        }
      }


      // Add note selection.
      // -----------------------------------------------------------------
      $note_use_options = array(
        'intern' => 'Intern',
        'telefoongesprek' => 'Telefoongesprek',
        'algemeen' => 'Algemeen',
      );

      $form['note_select'] = array(
        '#type' => 'fieldset',
        '#title' => 'Notities.',
        '#collapsible' => TRUE,
        '#collapsed' => TRUE,
      );

      // Show stored notes.
      if (!empty($form_state['storage']['notes'])) {
        $form['note_select']['#collapsed'] = FALSE;
        $markup = '<p><strong>Geselecteerd:</strong><br/>';
        foreach ($form_state['storage']['notes'] as $key => $note) {
          $fieldlist = array();
          $uselist = array();
          foreach($note['note_fields'] as $field) {
            $fieldlist[] = check_plain($webform_component_select_options[$field]);
          }
          foreach($note['note_use'] as $note_use) {
            $uselist[] = check_plain($note_use_options[$note_use]);
          }
          $markup .= '<p><strong>Notitie ' . ($key + 1) . '</strong><br/>Velden: ' . implode(', ', $fieldlist) . '<br/>';
          $markup .= 'Gebruik: ' . implode(', ', $uselist) . '<br/></p>';
        }
      }
      else {
        $markup = '';
      }

      $form['note_select']['selected_notes'] = array(
        '#type' => 'markup',
        '#markup' => $markup,
      );

      $form['note_select']['note_fields'] = array(
        '#type' => 'select',
        '#title' => 'Notitie velden.',
        '#options' => $webform_component_select_options,
        '#multiple' => TRUE,
        '#empty_option' => '- Geen -',
      );

      $form['note_select']['note_use'] = array(
        '#type' => 'select',
        '#title' => 'Notitie gebruik',
        '#options' => $note_use_options,
        '#multiple' => TRUE,
        '#empty_option' => '- Geen -',
      );

      $form['note_select']['add_note'] = array(
        '#type' => 'button',
        '#value' => 'Voeg notitie toe',
        '#name' => 'add_note',
        '#ajax' => array(
          'wrapper' => 'spwebformsync_ajax_form',
          'callback' => 'spwebformsync_ajax_form_callback',
        ),
      );

      $form['note_select']['reset_notes'] = array(
        '#type' => 'button',
        '#value' => 'Leeg selectie',
        '#name' => 'reset_notes',
        '#ajax' => array(
          'wrapper' => 'spwebformsync_ajax_form',
          'callback' => 'spwebformsync_ajax_form_callback',
        ),
      );

      // Add group selection.
      // -----------------------------------------------------------------

      // Make sure the groups are cached before autocompleting.
      $groups = spwebformsync_get_civi_groups();

      $form['fixed_group_select'] = array(
        '#type' => 'fieldset',
        '#title' => 'Selecteer CiviCRM groep(en) waar het contact altijd aan moet worden toegevoegd (optioneel)',
        '#collapsible' => TRUE,
        '#collapsed' => TRUE,
      );
      // Show message.
      $form['fixed_group_select']['message_cache'] = array(
        '#type' => 'markup',
        '#markup' => '<p><strong>Leeg de Drupal cache als een CiviCRM groep niet te vinden is!</strong></p>',
      );
      // Show stored groups.
      if (!empty($form_state['storage']['selected_fixed_groups'])) {
        $form['fixed_group_select']['#collapsed'] = FALSE;
        $markup = '<p><strong>Geselecteerd:</strong><br/>';
        foreach ($form_state['storage']['selected_fixed_groups'] as $key => $group) {
          $markup .= $group . '<br/>';
        }
      }
      else {
        $markup = '';
      }
      $form['fixed_group_select']['selected_fixed_groups'] = array(
        '#type' => 'markup',
        '#markup' => $markup,
      );
      $form['fixed_group_select']['fixed_group'] = array(
        '#type' => 'textfield',
        '#title' => 'Selecteer groep',
        '#autocomplete_path' => 'spwebformsync/group/autocomplete',
      );

      $form['fixed_group_select']['add_fixed_group'] = array(
        '#type' => 'button',
        '#value' => 'Voeg toe',
        '#name' => 'add_fixed_group',
        '#ajax' => array(
          'wrapper' => 'spwebformsync_ajax_form',
          'callback' => 'spwebformsync_ajax_form_callback',
        ),
      );

      $form['fixed_group_select']['reset_fixed_groups'] = array(
        '#type' => 'button',
        '#value' => 'Leeg selectie',
        '#name' => 'reset_fixed_groups',
        '#ajax' => array(
          'wrapper' => 'spwebformsync_ajax_form',
          'callback' => 'spwebformsync_ajax_form_callback',
        ),
      );

      // Variable group syncs.
      // --------------------------------------------------------------------
      $form['variable_group_select'] = array(
        '#type' => 'fieldset',
        '#title' => 'Synchroniseer webformulier selectie veldwaarden naar CiviCRM groepen (optioneel)',
        '#collapsible' => TRUE,
        '#collapsed' => TRUE,
      );
      // Get webform select components.
      $webform_components = spwebformsync_get_webform_allowed_components($webform_default, array('select' => 'select'));
      // Show selected fields.
      if (!empty($form_state['storage']['selected_webform_components'])) {
        $form['variable_group_select']['#collapsed'] = FALSE;
        foreach ($form_state['storage']['selected_webform_components'] as $component_key => $component_values) {
          // Remove already selected components from component list.
          unset($webform_components[$component_key]);
          $form['variable_group_select']['component_'.$component_key] = array(
            '#type' => 'fieldset',
            '#title' => 'Webform veld: '.$component_values['name'],
          );
          // Get webform_component options.
          $component_options = spwebformsync_parse_webform_select_options($webform, $component_key);
          foreach ($component_options as $key => $name) {
            if (!empty($sync_data[$webform->nid]['selected_variable_groups'][$component_key]['values'][$key])) {
              $var_values = $sync_data[$webform->nid]['selected_variable_groups'][$component_key]['values'][$key];
              $default = $var_values['group_name'] . ' (' . $var_values['group_id'] . ')';
            }
            else {
              $default = '';
            }
            // Get default value.
            $form['variable_group_select']['component_'.$component_key]['component_'.$component_key.'_value_'.$key] = array(
              '#type' => 'textfield',
              '#title' => '"'.$name.'" toevoegen aan CiviCRM groep:',
              '#autocomplete_path' => 'spwebformsync/group/autocomplete',
              '#default_value' => $default,
            );
            $form['variable_group_select']['component_'.$component_key]['del_component_'.$component_key] = array(
              '#type' => 'button',
              '#value' => 'Verwijder',
              '#name' => 'del_component_'.$component_key,
              '#weight' => 99,
              '#ajax' => array(
                'wrapper' => 'spwebformsync_ajax_form',
                'callback' => 'spwebformsync_ajax_form_callback',
              ),
            );
          }
        }
      }
      if (!empty($webform_components)) {
        $webform_component_select_options = spwebformsync_get_components_list($webform_components);
        $form['variable_group_select']['webform_comp'] = array(
          '#type' => 'select',
          '#title' => 'Formulier veld',
          '#options' => $webform_component_select_options,
          '#empty_option' => '- Geen -',
        );
        $form['variable_group_select']['add_webform_comp'] = array(
          '#type' => 'button',
          '#value' => 'Voeg toe',
          '#name' => 'add_webform_comp',
          '#ajax' => array(
            'wrapper' => 'spwebformsync_ajax_form',
            'callback' => 'spwebformsync_ajax_form_callback',
          ),
        );
      }
      else {
        $form['variable_group_select']['variable_group_select_no_components'] = array(
          '#markup' => '<p>Er zijn geen geschikte webformulier velden (meer) beschikbaar.</p>',
        );
      }

      // SP Refer sync.
      // ----------------------------------------------------------------------------------
      $webform_components = spwebformsync_get_webform_allowed_components($webform_default, $allowed_types);
      if (!empty($webform_components)) {
        $webform_component_select_options = spwebformsync_get_components_list($webform_components);
        $form['sprefer_fieldset'] = array(
          '#type' => 'fieldset',
          '#title' => 'SP Referal formulier synchronisatie (optioneel)',
          '#description' => 'Selecteer het veld waarin het CiviCRM contactnummer dat in de url van het formulier is meegegeven wordt opgeslagen. Kies vervolgens de groep waar dit contact in CiviCRM moet worden toegevoegd.',
          '#collapsible' => TRUE,
          '#collapsed' => TRUE,
        );

        if (!empty($sync_data[$webform->nid]['sprefer_field'])) {
          $default = $sync_data[$webform->nid]['sprefer_field'];
          $form['sprefer_fieldset']['#collapsed'] = FALSE;
        }
        else {
          $default = '';
        }
        $form['sprefer_fieldset']['sprefer_field'] = array(
          '#type' => 'select',
          '#title' => 'Formulier veld',
          '#options' => $webform_component_select_options,
          '#empty_option' => '- Geen -',
          '#default_value' => $default,
        );

        if (!empty($sync_data[$webform->nid]['sprefer_group'])) {
          $sprefer_group = $sync_data[$webform->nid]['sprefer_group'];
          $default = $groups[$sprefer_group] . ' (' . $sprefer_group . ')';
        }
        else {
          $default = '';
        }
        $form['sprefer_fieldset']['sprefer_group'] = array(
          '#type' => 'textfield',
          '#title' => 'Selecteer groep',
          '#autocomplete_path' => 'spwebformsync/group/autocomplete',
          '#default_value' => $default,
        );
      }

      $form['submit'] = array(
        '#type'  => 'submit',
        '#name' => 'spcontact_submit',
        '#value' => 'Sla op',
      );
    }
    else {
      $form['no_components'] = array(
        '#markup' => '<p>Er zijn geen geschikte webformulier velden (meer) beschikbaar. (Zijn de gewenste veldtypes <a href="/admin/config/sp/spwebformsync/settings">geselecteerd</a>?)</p>',
      );
    }

  }
  return $form;
}

function spwebformsync_ajax_form_callback($form, &$form_state) {
  return $form;
}

function spwebformsync_admin_form_validate($form, &$form_state) {
  $storage = &$form_state['storage'];
  $values = $form_state['values'];
  $webform = $values['webform'];
  if (!empty($form_state['triggering_element']['#name'])) {
    $trigger = $form_state['triggering_element']['#name'];
    switch ($trigger) {
    case 'webform':
      // Set stored values if available.
      spwebformsync_store_sync_data($form_state, $webform);
      return;
      break;
    case 'add_note':
      // Store selected group.
      if (!empty($form_state['values']['note_fields'])) {
        $form_state['storage']['notes'][] = array(
          'note_fields' => $form_state['values']['note_fields'],
          'note_use' => $form_state['values']['note_use'],
        );
        $form_state['input']['note_fields'] = '';
        $form_state['input']['note_use'] = '';
      }
      $form_state['rebuild'] = TRUE;
      return;
      break;
    case 'reset_notes':
      unset($storage['notes']);
      $form_state['rebuild'] = TRUE;
      return;
      break;
    case 'reset_fixed_groups':
      unset($storage['selected_fixed_groups']);
      $form_state['rebuild'] = TRUE;
      return;
      break;
    case 'add_fixed_group':
      $groups = spwebformsync_get_civi_groups();
      // Store selected group.
      if (!empty($form_state['values']['fixed_group'])) {
        $group_raw = $form_state['values']['fixed_group'];
        $group = spwebformsync_parse_group_form_select_value($group_raw);
        if (!empty($group)) {
          $form_state['storage']['selected_fixed_groups'][$group] = $groups[$group];
          // Empty group field.
          $form_state['input']['fixed_group'] = '';
        }
      }
      $form_state['rebuild'] = TRUE;
      return;
      break;
    case 'add_webform_comp':
      $webform_components = spwebformsync_get_webform_allowed_components($form_state['values']['webform'], array('select' => 'select'));
      $webform_component_select_options = webform_component_list($webform, $webform_components, FALSE);
      // Store selected webform component.
      if (!empty($form_state['values']['webform_comp'])) {
        $component_id = $form_state['values']['webform_comp'];
        $storage['selected_webform_components'][$component_id]['name'] = $webform_component_select_options[$component_id];
        // Empty group field.
        unset($form_state['input']['webform_comp']);
      }
      $form_state['rebuild'] = TRUE;
      return;
      break;
    }

    // Delete variable group syncs button.
    preg_match('/^del_component_([0-9]+)$/', $trigger, $matches);
    if (!empty($matches[1])) {
      $sync_data = variable_get('spwebformsync_sync_data', array());
      unset($storage['selected_webform_components'][$matches[1]]);
      unset($sync_data[$webform]['selected_variable_groups'][$matches[1]]);
      variable_set('spwebformsync_sync_data', $sync_data);
    }
  }

  // Validate form.
  $values = $form_state['values'];
  if (
    empty($values['contact_id_component']) &&
    empty($values['email_component']) &&
    empty($values['telephone_component']) &&
    (
      (
        empty($values['postal_code_component']) ||
        empty($values['house_number_component'])
      ) &&
      (
        empty($values['street_component']) ||
        empty($values['house_number_component']) ||
        empty($values['locality_component'])
      )
    )
  ) {
  form_set_error('fields', 'Kies een combinatie van velden zodat een contact te bereiken is (telefoon, e-mail, huis)!');
  }
}

function spwebformsync_admin_form_submit($form, &$form_state) {
  $values = $form_state['values'];
  $groups = spwebformsync_get_civi_groups();
  $webform = node_load($form_state['values']['webform']);
  $sync_data = variable_get('spwebformsync_sync_data', array());
  // Store data.
  $static_fields = array(
    'webform',
    'sprefer_field',
  );
  $storage_values = array(
    'selected_fixed_groups',
  );
  foreach ($static_fields as $key) {
    if (isset($values[$key])) {
      $sync_data[$values['webform']][$key] = $values[$key];
    }
  }
  foreach ($storage_values as $key) {
    if (isset($form_state['storage'][$key])) {
      $sync_data[$values['webform']][$key] = $form_state['storage'][$key];
    }
    else {
      unset($sync_data[$values['webform']][$key]);
    }
  }
  // Store selected components to sync with CiviCRM contact data.
  foreach ($values as $key => $value) {
    if (preg_match('/^[a-z_]+_component$/', $key)) {
      $sync_data[$values['webform']][$key] = $value;
    }
  }
  // Store overwrite.
  if (isset($values['overwrite'])) {
    $sync_data[$values['webform']]['overwrite'] = $values['overwrite'];
  }
  // Store sprefer group.
  $group = spwebformsync_parse_group_form_select_value($values['sprefer_group']);
  $sync_data[$values['webform']]['sprefer_group'] = $group;

  // Store notes.
  $sync_data[$values['webform']]['notes'] = empty($form_state['storage']['notes']) ? array() : $form_state['storage']['notes'];

  // Store selected variable group syncs.
  foreach ($values as $key => $value) {
    if (!empty($value)) {
      $result = preg_match('/^component_(.*)_value_(.*)$/', $key, $matches);
      if (!empty($result)) {
        $component_id = $matches[1];
        $selected_value_id = $matches[2];
        $component_options = spwebformsync_parse_webform_select_options($webform, $component_id);
        $group = spwebformsync_parse_group_form_select_value($value);
        $sync_data[$values['webform']]['selected_variable_groups'][$component_id]['component_id'] = $component_id;
        $sync_data[$values['webform']]['selected_variable_groups'][$component_id]['component_name'] = $webform->webform['components'][$component_id]['name'];
        $sync_data[$values['webform']]['selected_variable_groups'][$component_id]['values'][$selected_value_id] = array(
          'value_id' => $selected_value_id,
          'value_name' => $component_options[$selected_value_id],
          'group_id' => $group,
          'group_name' => $groups[$group],
        );
      }
    }
  }
  variable_set('spwebformsync_sync_data', $sync_data);
  drupal_set_message('SP webformulier synchronisatie is toegevoegd.');
  $form_state['redirect'] = 'admin/config/sp/spwebformsync';
}

function spwebformsync_settings_form($form, &$form_state) {
  // Synchronization type.
  if (empty(variable_get('spwebformsync_sync_type', FALSE))) {
    variable_set('spwebformsync_sync_type', 'cron');
  }
  if (empty(variable_get('spwebformsync_sync_target', FALSE))) {
    variable_set('spwebformsync_sync_target', 'civicrm');
  }
  $form['spwebformsync_sync_settings'] = array(
    '#type' => 'fieldset',
    '#title' => 'Sync instellingen',
  );

  $form['spwebformsync_sync_settings']['spwebformsync_sync_target'] = array(
    '#type' => 'radios',
    '#title' => 'Waar wilt u de data heensturen?',
    '#default_value' => variable_get('spwebformsync_sync_target', array()),
    '#options' => array('civicrm' => 'Direct naar CiviCRM', 'datadrop' => 'Naar SP datadrop, waar e.e.a. verder verwerkt wordt'),
    '#required' => TRUE,
  );

  $form['spwebformsync_sync_settings']['spwebformsync_datadrop_server'] = array(
    '#type' => 'textfield',
    '#title' => 'Datadrop server (url)',
    '#default_value' => variable_get('spwebformsync_datadrop_server', ''),
    '#states' => array(
      'visible' => array(
        ':input[name="spwebformsync_sync_target"]' => array('value' => 'datadrop'),
      ),
      'required' => array(
        ':input[name="spwebformsync_sync_target"]' => array('value' => 'datadrop'),
      ),
    ),
  );

  $form['spwebformsync_sync_settings']['spwebformsync_datadrop_secret'] = array(
    '#type' => 'textfield',
    '#title' => 'Geheime sleutel',
    '#description' => 'This secret must match the secret set in the spdatadrop module on the datadrop server.',
    '#default_value' => variable_get('spwebformsync_datadrop_secret', ''),
    '#states' => array(
      'visible' => array(
        ':input[name="spwebformsync_sync_target"]' => array('value' => 'datadrop'),
      ),
      'required' => array(
        ':input[name="spwebformsync_sync_target"]' => array('value' => 'datadrop'),
      ),
    ),
  );

  $form['spwebformsync_sync_settings']['spwebformsync_datadrop_secret_unsubscribe'] = array(
    '#type' => 'textfield',
    '#title' => 'Geheime sleutel voor uitschrijf link',
    '#description' => 'This secret must match the unsubscribe secret set in the spdatadrop module on the datadrop server.',
    '#default_value' => variable_get('spwebformsync_datadrop_secret_unsubscribe', ''),
    '#states' => array(
      'visible' => array(
        ':input[name="spwebformsync_sync_target"]' => array('value' => 'datadrop'),
      ),
      'required' => array(
        ':input[name="spwebformsync_sync_target"]' => array('value' => 'datadrop'),
      ),
    ),
  );

  $form['spwebformsync_sync_settings']['spwebformsync_sync_type'] = array(
    '#type' => 'radios',
    '#title' => 'Wanneer wilt u contacten synchroniseren?',
    '#default_value' => variable_get('spwebformsync_sync_type', array()),
    '#options' => array('direct' => 'Direct (geen controle bevestiging inzending!)', 'cron' => 'Cron', 'manual' => 'Handmatig'),
    '#required' => TRUE,
  );

  // Max sync items per cron.
  $form['spwebformsync_sync_settings']['spwebformsync_cron_number'] = array(
    '#type' => 'select',
    '#title' => 'Hoeveel webformulier inzendingen moeten per keer gesynchroniseerd worden?',
    '#default_value' => variable_get('spwebformsync_cron_number', 0),
    '#options' => range(0, 30, 1),
    '#states' => array(
      'invisible' => array(
        ':input[name="spwebformsync_sync_type"]' => array('value' => 'direct'),
      ),
    ),
  );

  // Check confirmation.
  $form['spwebformsync_sync_settings']['spwebformsync_do_not_check_confirmation'] = array(
    '#type' => 'checkbox',
    '#title' => 'Sla de bevestiginscontrole voor inzendingen over',
    '#default_value' => variable_get('spwebformsync_do_not_check_confirmation', 0),
    '#states' => array(
      'invisible' => array(
        ':input[name="spwebformsync_sync_type"]' => array('value' => 'direct'),
      ),
    ),
  );

  $form['spwebformsync_sync_settings']['spwebformsync_sync_now'] = array(
    '#type' => 'button',
    '#value' => 'Nu synchroniseren',
    '#name' => 'sync_now',
    '#states' => array(
      'visible' => array(
        ':input[name="spwebformsync_sync_type"]' => array('value' => 'manual'),
      ),
    ),
    '#ajax' => array(
      'wrapper' => 'sync_now',
      'callback' => 'spwebformsync_form_sync',
    ),
    '#prefix' => '<div id="sync_now">',
    '#suffix' => '</div>',
  );

  // Webform settings.
  $form['spwebformsync_webform_settings'] = array(
    '#type' => 'fieldset',
    '#title' => 'Webformulier instellingen',
  );
  // Get webform component options.
  $component_types = module_invoke_all('webform_component_info');
  // Remove sp_active component type, this will always be synced when available.
  if (!empty($component_types['sp_active'])) {
    unset($component_types['sp_active']);
  }
  // Remove sp_work_and_int component type, always synced when available.
  if (!empty($component_types['sp_work_and_int'])) {
    unset($component_types['sp_work_and_int']);
  }

  $options = array();
  if (!empty($component_types)) {
    foreach ($component_types as $key => $component_type) {
      $options[$key] = $component_type['label'];
    }
  }
  else {
    drupal_set_message('Er is geen webformulier veldtype gevonden.', 'error');
  }
  // Select webform component types form.
  $form['spwebformsync_webform_settings']['spwebformsync_allowed_component_types'] = array(
    '#type' => 'checkboxes',
    '#title' => 'Kies webformulier veld types om te kunnen synchroniseren.',
    '#options' => $options,
    '#default_value' => variable_get('spwebformsync_allowed_component_types', array()),
  );

  $webforms = spwebformsync_get_webforms();
  if (!empty($webforms)) {
    foreach ($webforms as $webform) {
      $webform_select_options[$webform->nid] = $webform->title;
    }

    // Add existing form submissions.
    $form['spwebformsync_add_submissions'] = array(
      '#type' => 'fieldset',
      '#title' => 'Inzendingen toevoegen aan sync wachtrij',
      '#description' => 'Voeg formulierinzendingen toe aan de synchronisatie wachtrij die reeds bestonden vóór het aanmaken van de synchronisatie.',
      '#prefix' => '<div id="spwebformsync_add_submissions">',
      '#suffix' => '</div>',
    );
    $form['spwebformsync_add_submissions']['spwebformsync_add_submissions_select'] = array(
      '#type' => 'select',
      '#title' => 'Selecteer het webformulier',
      '#options' => $webform_select_options,
      '#empty_option' => '- Geen -',
      '#name' => 'spwebformsync_add_submissions_select',
    );
    $form['spwebformsync_add_submissions']['spwebformsync_add_submissions_submit'] = array(
      '#type' => 'button',
      '#value' => 'Toevoegen',
      '#name' => 'add_submissions_submit',
      '#states' => array(
        'visible' => array(
          ':input[name="spwebformsync_add_submissions_select"]' => array('!value' => ''),
        ),
      ),
      '#ajax' => array(
        'wrapper' => 'spwebformsync_add_submissions',
        'callback' => 'spwebformsync_form_add_submissions',
      ),
    );

    // Resync all form submissions.
    $form['spwebformsync_resync_submissions'] = array(
      '#type' => 'fieldset',
      '#title' => 'Alle inzendingen opnieuw syncen',
      '#prefix' => '<div id="spwebformsync_resync_submissions">',
      '#suffix' => '</div>',
    );

    $form['spwebformsync_resync_submissions']['spwebformsync_resync_submissions_select'] = array(
      '#type' => 'select',
      '#title' => 'Selecteer het webformulier',
      '#options' => $webform_select_options,
      '#empty_option' => '- Geen -',
      '#name' => 'spwebformsync_resync_submissions_select',
    );

    $form['spwebformsync_resync_submissions']['spwebformsync_resync_submissions_submit'] = array(
      '#type' => 'button',
      '#value' => 'Sync opnieuw',
      '#name' => 'resync_submissions_submit',
      '#states' => array(
        'visible' => array(
          ':input[name="spwebformsync_resync_submissions_select"]' => array('!value' => ''),
        ),
      ),
      '#ajax' => array(
        'wrapper' => 'spwebformsync_resync_submissions',
        'callback' => 'spwebformsync_form_resync_submissions',
      ),
    );
  }
  return system_settings_form($form);
}

function spwebformsync_settings_form_validate($form, &$form_state) {
  $values = $form_state['values'];
}

function spwebformsync_form_sync($form, &$form_state) {
  $processed = spwebformsync_sync();
  if (!empty($processed)) {
    $text = '<div id="sync_now"><p>Inzendingen die gesynced zijn (ids): ' . implode(', ', $processed) . '</p>';
  }
  else {
    $text = '<div id="sync_now"><p>Geen inzendingen gevonden.</p>';
  }
  $form['spwebformsync_sync_settings']['spwebformsync_sync_now']['#prefix'] = $text;
  return $form['spwebformsync_sync_settings']['spwebformsync_sync_now'];
}

function spwebformsync_form_add_submissions($form, &$form_state) {
  $webform = $form_state['values']['spwebformsync_add_submissions_select'];
  $query = "SELECT wf.sid FROM {spwebformsync_syncstate} sp RIGHT JOIN {webform_submissions} wf ON sp.sid = wf.sid WHERE wf.nid = :webform AND id IS NULL AND submitted IS NOT NULL";
  $result = db_query($query, array(':webform' => $webform));
  if ($result) {
    $count = 0;
    while ($sid = $result->fetchField(0)) {
      // Add submission to list.
      $res = db_insert('spwebformsync_syncstate')
        ->fields(array(
          'sid' => $sid,
          'nid' => $webform,
          'state' => '0',
        ))
        ->execute();
      if (!empty($res)) {
        $count ++;
      }
    }
    drupal_set_message('Er zijn '.$count.' inzendingen toegevoegd aan de wachtrij.');
  }
  return $form['spwebformsync_add_submissions'];
}

function spwebformsync_form_resync_submissions($form, &$form_state) {
  $webform = $form_state['values']['spwebformsync_resync_submissions_select'];
  if (!empty($webform)) {
    $count = db_update('spwebformsync_syncstate')
      ->fields(array(
        'state' => 0,
      ))
      ->condition('nid', $webform)
      ->condition('state', array(1,2), 'IN')
      ->execute();
    drupal_set_message('Er zijn '.$count.' inzendingen toegevoegd aan de wachtrij.');
  }
  return $form['spwebformsync_resync_submissions'];
}

function spwebformsync_group_autocomplete($string) {
  $string = strtolower($string);
  $matches = array();
  if (strlen($string) > 2) {
    $matches = array();
    $groups = spwebformsync_get_civi_groups();

    foreach ($groups as $key => $group) {
      if (strpos(strtolower($group), $string) !== FALSE) {
        $matches[$group . ' (' . $key . ')'] = $group;
      }
      if (count($matches) > 10) break;
    }
  }

  //Return the result to the form in json
  drupal_json_output($matches);
}

function spwebformsync_overview() {
  $sync_data = variable_get('spwebformsync_sync_data', array());
  krsort($sync_data);

  // Show forms without sync.
  $markup = '<table><tr><th>Webformulieren zonder sync</th></tr>';
  $webforms = spwebformsync_get_webforms();
  krsort($webforms);
  foreach ($webforms as $form) {
    if (empty($sync_data[$form->nid]) && $form->status == 1) {
      $markup .= '<tr><td><a href="/node/'. $form->nid . '">'.$form->title.'</a></td></tr>';
    }
  }
  $markup .= '</table><p></p>&nbsp;</p>';

  // Show syncs.
  if (!empty($sync_data)) {
    $data_names = spwebformsync_civi_fields();
    $markup .= '<table><tr><th>Webformulier</th><th>Webformulier &#8594; Civicrm</th><th>Vaste groepen</th><th>Variabele groepselecties</th><th>Cron wachtrij</th><th></th></tr>';
    foreach ($sync_data as $data) {
      $form = node_load($data['webform']);
      if (!empty($form)) {
        $components = $form->webform['components'];
        $components_list = spwebformsync_get_components_list($components);
        $postphoned_number = spwebformsync_unprocessed_count($data['webform']);
        $markup .= '<tr><td><a href="/node/'. $form->nid . '">'.$form->title.'</a></td>';
        $markup .= '<td><table>';
        foreach ($data_names as $key => $name) {
          if (!empty($data[$key . '_component'])) {
            $component_id = $data[$key . '_component'];
            $component_name = !empty($components_list[$component_id]) ? $components_list[$component_id] : $components[$data[$key . '_component']]['name'];
            $markup .= '<tr><td>' . $component_name . '</td><td>&nbsp;&#8594;&nbsp;</td><td>' . ucfirst($name) . '</td></tr>';
          }
        }
        $markup .= '</table></td><td>';
        if (isset($data['selected_fixed_groups'])) {
          foreach ($data['selected_fixed_groups'] as $groupname) {
            $markup .= $groupname . '<br/>';
          }
        }
        else {
          $markup .= 'Geen';
        }
        $markup .= '</td><td>';
        if (!empty($data['selected_variable_groups'])) {
          foreach ($data['selected_variable_groups'] as $component_data) {
            $markup .= '<p>'.$component_data['component_name'].'<br/><table>';
            foreach ($component_data['values'] as $sync_items) {
              $markup .= '<tr><td>'.$sync_items['value_name'].'</td><td>&nbsp;&#8594;&nbsp;</td><td>'.$sync_items['group_name'].'</td></tr>';
            }
            $markup .= '</table></p>';
          }
        }
        else {
          $markup .= 'Geen';
        }
        $markup .= '</td><td>' . $postphoned_number . '</td>';
        $markup .= '<td><a href="/admin/config/sp/spwebformsync/configure/' . $form->nid . '">bewerk</a> <a href="/admin/config/sp/spwebformsync/delete/' . $form->nid . '">verwijder</a></td></tr>';
      }
    }
    $markup .= '</table>';
  }
  else {
    $markup = 'Er zijn nog geen SP <a href="/admin/config/sp/spwebformsync/add">webformulier synchronisaties</a> toegevoegd.';
  }

  return $markup;
}

function spwebformsync_delete($id) {
  $sync_data = variable_get('spwebformsync_sync_data', array());
  $result = spwebformsync_delete_all_webform_submissions($id);
  if (!empty($result)) {
    drupal_set_message('Cron wachtrij webformulier synchronisatie nr. ' . $id . ' is verwijderd.');
  }
  if (isset($sync_data[$id])) {
    unset($sync_data[$id]);
    variable_set('spwebformsync_sync_data', $sync_data);
    drupal_set_message('SP webformulier synchronisatie nr. ' . $id . ' is verwijderd.');
  }
  drupal_goto('/admin/config/sp/spwebformsync');
}

function spwebformsync_store_sync_data(&$form_state, $webform) {
  $storage = &$form_state['storage'];
  if (!empty($webform)) {
    $sync_data = variable_get('spwebformsync_sync_data', array());
    if (!empty($sync_data[$webform]['notes'])) {
      $storage['notes'] = $sync_data[$webform]['notes'];
    }
    if (!empty($sync_data[$webform]['selected_fixed_groups'])) {
      $storage['selected_fixed_groups'] = $sync_data[$webform]['selected_fixed_groups'];
    }
    if (!empty($sync_data[$webform]['selected_variable_groups'])) {
      foreach ($sync_data[$webform]['selected_variable_groups'] as $data) {
        $storage['selected_webform_components'][$data['component_id']]['name'] = $data['component_name'];
      }
    }
  }
}
