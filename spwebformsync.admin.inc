<?php

function spwebformsync_admin_form($form, &$form_state) {
  //dpm($form_state);
  //dpm($form_state['storage']);
  $values = empty($form_state['values']) ? array() : $form_state['values'];
  $sync_data = variable_get('spwebformsync_sync_data', array());

  $form = array();

  $form['#prefix'] = '<div id="spwebformsync_ajax_form">';
  $form['#suffix'] = '</div>';

  // Select webform element.
  // ----------------------------------------------------------

  // Get data.
  $webforms = spwebformsync_get_webforms();
  if (!empty($webforms)) {
    foreach ($webforms as $webform) {
      $webform_select_options[$webform->nid] = $webform->title;
    }

    // Set default value.
    $webform_default = isset($values['webform']) ? $values['webform'] : key($webform_select_options);

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

  if (isset($values['webform'])) {
    // Set default value.
    $form['webform']['#default_value'] = $values['webform'];
    unset($form['webform']['#empty_option']);


    $allowed_types = variable_get('spwebformsync_allowed_component_types', array());
 
    $data_names = spwebformsync_get_data_names();
    $webform_components = spwebformsync_get_webform_allowed_components($webform_default, $allowed_types);
    if (!empty($webform_components)) {

      $webform = node_load($webform_default);
      $webform_component_select_options = webform_component_list($webform, $webform_components, FALSE);

      // Add select field components.
      // ----------------------------------------------------------
      $form['fields'] = array(
        '#type' => 'fieldset',
        '#title' => 'Selecteer naar het CiviCRM contact te synchronizeren webformulier velden (optioneel)',
      );

      foreach ($data_names as $data_key => $data_name) {
        if (!empty($sync_data[$webform->nid][$data_key . '_component'])) {
          $default_value = $sync_data[$webform->nid][$data_key . '_component'];
        }
        else {
          $default_value = '';
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

      // Add group selection.
      // -----------------------------------------------------------------

      // Make sure the groups are cached before autocompleting.
      $groups = spwebformsync_get_civi_groups();

      $form['fixed_group_select'] = array(
        '#type' => 'fieldset',
        '#title' => 'Selecteer CiviCRM groep(en) waar het contact altijd aan moet worden toegevoegd (optioneel)',
      );
      // Show message.
      $form['fixed_group_select']['message_cache'] = array(
        '#type' => 'markup',
        '#markup' => '<p><strong>Leeg de Drupal cache als een CiviCRM groep niet te vinden is!</strong></p>',
      );
      // Show stored groups.
      if (!empty($form_state['storage']['selected_fixed_groups'])) {
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
        '#title' => 'Synchronizeer webformulier selectie veldwaarden naar CiviCRM groepen (optioneel)',
      );
      // Get webform select components.
      $webform_components = spwebformsync_get_webform_allowed_components($webform_default, array('select' => 'select'));
      // Show selected fields.
      if (!empty($form_state['storage']['selected_webform_components'])) {
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
          }
        }
      }
      if (!empty($webform_components)) {
        $webform_component_select_options = webform_component_list($webform, $webform_components, FALSE);
        $form['variable_group_select']['webform_component'] = array(
          '#type' => 'select',
          '#title' => 'Formulier veld',
          '#options' => $webform_component_select_options,
          '#empty_option' => '- Geen -',
        );
        $form['variable_group_select']['add_webform_component'] = array(
          '#type' => 'button',
          '#value' => 'Voeg toe',
          '#name' => 'add_webform_component',
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
    switch ($form_state['triggering_element']['#name']) {
    case 'webform':
      // Set stored values if available.
      if (!empty($webform)) {
        $sync_data = variable_get('spwebformsync_sync_data', array());
        if (!empty($sync_data[$webform]['selected_fixed_groups'])) {
          $storage['selected_fixed_groups'] = $sync_data[$webform]['selected_fixed_groups'];
        }
        if (!empty($sync_data[$webform]['selected_variable_groups'])) {
          foreach ($sync_data[$webform]['selected_variable_groups'] as $data) {
            $storage['selected_webform_components'][$data['component_id']]['name'] = $data['component_name'];
          }
        }
      }
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
    case 'add_webform_component':
      $webform_components = spwebformsync_get_webform_allowed_components($form_state['values']['webform'], array('select' => 'select'));
      $webform_component_select_options = webform_component_list($webform, $webform_components, FALSE);
      // Store selected webform component.
      if (!empty($form_state['values']['webform_component'])) {
        $component_id = $form_state['values']['webform_component'];
        $storage['selected_webform_components'][$component_id]['name'] = $webform_component_select_options[$component_id];
        // Empty group field.
        unset($form_state['input']['webform_component']);
      }
      $form_state['rebuild'] = TRUE;
      return;
      break;
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
  }
  // Store selected components to sync with CiviCRM contact data.
  foreach ($values as $key => $value) {
    if (strpos($key, '_component') !== FALSE) {
      $sync_data[$values['webform']][$key] = $value;
    }
  }
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
  drupal_set_message('SP weboformulier synchronisatie is toegevoegd.');
  $form_state['redirect'] = 'admin/config/sp/spwebformsync';
}

function spwebformsync_settings_form($form, &$form_state) {
  // Synchronization type.
  if (empty(variable_get('spwebformsync_sync_type', FALSE))) {
    variable_set('spwebformsync_sync_type', 'cron');
  }
  $form['spwebformsync_sync_settings'] = array(
    '#type' => 'fieldset',
    '#title' => 'Cron instellingen',
  );
  $form['spwebformsync_sync_settings']['spwebformsync_sync_type'] = array(
    '#type' => 'radios',
    '#title' => 'Hoe wilt u contacten synchroniseren?',
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
    '#title' => 'Kies webformulier tekst veld types om te kunnen synchroniseren.',
    '#options' => $options,
    '#default_value' => variable_get('spwebformsync_allowed_component_types', array()),
  );

  // Civicrm settings.
  $form['spwebformsync_civicrm_settings'] = array(
    '#type' => 'fieldset',
    '#title' => 'CiviCRM synchronisatie instellingen',
  );
  $tags = spwebformsync_get_civi_tags();
  if (empty(variable_get('spwebformsync_unverified_tag', FALSE)) && $id = array_search('Ongecontroleerd', $tags)) {
    variable_set('spwebformsync_unverified_tag', $id);
  }
  $form['spwebformsync_civicrm_settings']['spwebformsync_unverified_tag'] = array(
    '#type' => 'select',
    '#title' => 'Kies de tag die moet worden toegevoegd aan contacten die handmatig moeten worden gecontroleerd.',
    '#options' => $tags,
    '#empty_option' => '- Geen -',
    '#default_value' => variable_get('spwebformsync_unverified_tag', array()),
  );
  return system_settings_form($form);
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
  if (!empty($sync_data)) {
    $data_names = spwebformsync_get_data_names();
    $markup = '<table><tr><th>Webformulier</th><th>Webformulier &#8594; Civicrm</th><th>Vaste groepen</th><th>Variabele groepselecties</th><th>Cron wachtrij</th><th></th></tr>';
    foreach ($sync_data as $data) {
      $form = node_load($data['webform']);
      $postphoned_number = spwebformsync_unprocessed_count($data['webform']);
      $markup .= '<tr><td><a href="/node/'. $form->nid . '">'.$form->title.'</a></td>';
      $markup .= '<td><table>';
      foreach ($data_names as $key => $name) {
        if (!empty($data[$key . '_component'])) {
          $markup .= '<tr><td>' . $form->webform['components'][$data[$key . '_component']]['name'] . '</td><td>&nbsp;&#8594;&nbsp;</td><td>' . ucfirst($name) . '</td></tr>';
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
          foreach ($component_data['values'] as $sync_data) {
            $markup .= '<tr><td>'.$sync_data['value_name'].'</td><td>&nbsp;&#8594;&nbsp;</td><td>'.$sync_data['group_name'].'</td></tr>';
          }
          $markup .= '</table></p>';
        }
      }
      else {
        $markup .= 'Geen';
      }
      $markup .= '</td><td>' . $postphoned_number . '</td>';
      $markup .= '<td><a href="/admin/config/sp/spwebformsync/delete/' . $form->nid . '">verwijder</a></td></tr>';
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
