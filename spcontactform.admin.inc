<?php

function spcontactform_admin_form($form, &$form_state) {
  //dpm($form_state);
  //dpm($form_state['storage']);
  $form = array();

  $form['#prefix'] = '<div id="spcontactform_ajax_form">';
  $form['#suffix'] = '</div>';


  // Select webform element.
  // ----------------------------------------------------------

  // Get data.
  $webforms = spcontactform_get_webforms();
  foreach ($webforms as $webform) {
    $webform_select_options[$webform->nid] = $webform->title;
  }

  // Set default value.
  $webform_default = isset($form_state['values']['webform']) ? $form_state['values']['webform'] : key($webform_select_options);

  // Create form element.
  $form['webform'] = array(
    '#type' => 'select',
    '#title' => 'Kies het formulier',
    '#options' => $webform_select_options,
    '#empty_option' => 'Kies:',
    '#ajax' => array(
      'wrapper' => 'spcontactform_ajax_form',
      'callback' => 'spcontactform_ajax_form_callback',
    ),
  );

  if (isset($form_state['values']['webform'])) {
    // Set default value.
    $form['webform']['#default_value'] = $form_state['values']['webform'];
    unset($form['webform']['#empty_option']);

    // Add select field components.
    // ----------------------------------------------------------

    $data_names = spcontactform_get_data_names();
    $allowed_types = variable_get('spcontactform_allowed_component_types', array());
    $webform_components = spcontactform_get_webform_allowed_components($webform_default, $allowed_types);
    if (!empty($webform_components)) {
      $webform = node_load($webform_default);
      $webform_component_select_options = webform_component_list($webform, $webform_components, FALSE);
      $form['fields'] = array(
        '#type' => 'fieldset',
        '#title' => 'Selecteer te synchronizeren formulier velden',
      );

      foreach ($data_names as $data_key => $data_name) {
        // Select name component.
        $form['fields'][$data_key . '_component'] = array(
          '#type' => 'select',
          '#title' => ucfirst($data_name),
          '#options' => $webform_component_select_options,
          '#empty_option' => '- Geen -',
          '#ajax' => array(
            'wrapper' => 'spcontactform_ajax_form',
            'callback' => 'spcontactform_ajax_form_callback',
          ),
        );

        // Set default value.
        if (isset($form_state['values'][$data_key . '_component'])) {
          $form['fields'][$data_key . '_component']['#default_value'] = $form_state['values'][$data_key . '_component'];
        }

      }
      // Add group selection.
      // -----------------------------------------------------------------

      // Make sure the groups are cached before autocompleting.
      $groups = spcontactform_get_civi_groups();

      $form['group_select'] = array(
        '#type' => 'fieldset',
        '#title' => 'Selecteer groep(en) waar contact aan moet worden toegevoegd',
      );
      // Store selected group.
      if (!empty($form_state['values']['groups'])) {
        $group_raw = $form_state['values']['groups'];
        preg_match('@^.+\(([0-9]+)\)$@', $group_raw, $matches);
        if (!empty($matches[1])) {
          $group = $matches[1];
          $form_state['storage']['selected_groups'][$group] = $groups[$group];
          // Empty group field.
          $form_state['input']['groups'] = '';
        }
      }
      // Show message.
      $form['group_select']['message_cache'] = array(
        '#type' => 'markup',
        '#markup' => '<p>Leeg de Drupal cache als een CiviCRM groep niet te vinden is.</p>',
      );
      // Show stored groups.
      if (!empty($form_state['storage']['selected_groups'])) {
        $markup = '<p><strong>Geselecteerd:</strong><br/>';
        foreach ($form_state['storage']['selected_groups'] as $key => $group) {
          $markup .= $group . '<br/>';
        }
      }
      else {
        $markup = '';
      }
      $form['group_select']['selected_groups'] = array(
        '#type' => 'markup',
        '#prefix' => '<div id="spcontactform_selected_groups">',
        '#suffix' => '</div>',
        '#markup' => $markup,
      );
      $form['group_select']['groups'] = array(
        '#type' => 'textfield',
        '#title' => 'Selecteer groep',
        '#autocomplete_path' => 'spcontactform/group/autocomplete',
        '#ajax' => array(
          'wrapper' => 'spcontactform_ajax_form',
          'callback' => 'spcontactform_ajax_form_callback',
        ),
      );

      $form['group_select']['reset_groups'] = array(
        '#type' => 'submit',
        '#value' => 'Leeg selectie',
        '#name' => 'reset_groups',
        '#ajax' => array(
          'wrapper' => 'spcontactform_ajax_form',
          'callback' => 'spcontactform_ajax_form_callback',
        ),
      );

      // Keep notified of SP nieuws.
      // --------------------------------------------------------------------
      $form['hou_hoog'] = array(
        '#type' => 'fieldset', 
        '#title' => 'Hou op de hoogte van SP nieuws.',
      );

      $webform_components = spcontactform_get_webform_allowed_components($webform_default, array('select' => 'select'));
      if (!empty($webform_components)) {
        $webform_component_select_options = webform_component_list($webform, $webform_components, FALSE);
        if (!empty(variable_get('spcontactform_hou_hoog_group', FALSE))) {
          $form['hou_hoog']['hou_hoog_component'] = array(
            '#type' => 'select',
            '#title' => 'Formulier veld',
            '#options' => $webform_component_select_options,
            '#empty_option' => '- Geen -',
            '#ajax' => array(
              'wrapper' => 'spcontactform_ajax_form',
              'callback' => 'spcontactform_ajax_form_callback',
            ),
          );
          if (!empty($form_state['values']['hou_hoog_component'])) {
            $selected_component = $form_state['values']['hou_hoog_component'];
            $hou_hoog_accept_value_options = spcontactform_parse_webform_select_options($webform, $selected_component);
            $form['hou_hoog']['hou_hoog_accept_value'] = array(
              '#type' => 'select',
              '#title' => 'Bij formulier veld waarde',
              '#options' => $hou_hoog_accept_value_options,
              '#empty_option' => '- Geen -',
              '#ajax' => array(
                'wrapper' => 'spcontactform_ajax_form',
                'callback' => 'spcontactform_ajax_form_callback',
              ),
            );
          }
        }
        else {
          $form['hou_hoog']['hou_hoog_no_setting'] = array(
            '#markup' => '<p>Kies in de instellingen eerst een groep waaraan de deelnemers moeten worden toegevoegd die op de hoogte willen blijven van belangrijk SP nieuws.</p>', 
          );
        }
      }
      else {
        $form['hou_hoog']['hou_hoog_no_components'] = array(
          '#markup' => '<p>Er zijn geen geschikte webformulier velden gevonden.</p>', 
        );
      }
      $form['submit'] = array(
        '#type'  => 'submit',
        '#name' => 'spcontact_submit',
        '#value' => 'Voeg toe',
      );
    }
    else {
      $form['no_components'] = array(
        '#markup' => '<p>Er zijn geen geschikte webformulier velden gevonden.</p>', 
      );
    }

  }
  return $form;
}

function spcontactform_ajax_form_callback($form, &$form_state) {
  return $form;
}

function spcontactform_admin_form_validate($form, &$form_state) {
  // Empty selected groups if reset_groups button was clicked.
  if (
    !empty($form_state['triggering_element']['#name']) &&
    $form_state['triggering_element']['#name'] == 'reset_groups'
  ) {
  unset($form_state['storage']['selected_groups']);
  $form_state['rebuild'] = TRUE;
  return;
  }

  // Validate form.
  $values = $form_state['values'];
  if (
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
  if (empty($form_state['storage']['selected_groups'])) {
    form_set_error('group_select', 'Selecteer groep(en) waar contact aan moet worden toegevoegd!');
  }
  if (!empty($values['hou_hoog_component']) && empty($values['hou_hoog_accept_value'])) {
    form_set_error('hou_hoog', "Kies de conditie waarbij mensen op de hoogte willen blijven.");
  }
}

function spcontactform_admin_form_submit($form, &$form_state) {
  $values = $form_state['values'];
  $sync_data = variable_get('spcontactform_sync_data', array());
  // Store data.
  $static_fields = array(
    'webform',
    'hou_hoog_accept_value',
  );
  $storage_values = array(
    'selected_groups',
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
  foreach ($values as $key => $value) {
    if (strpos($key, '_component') !== FALSE) {
      $sync_data[$values['webform']][$key] = $value;
    }
  }
  variable_set('spcontactform_sync_data', $sync_data);
  drupal_set_message('SP Contactform synchronizatie is toegevoegd.');
  $form_state['redirect'] = 'admin/config/sp/spcontactform';
}

function spcontactform_settings_form($form, &$form_state) {
  // Synchronization type.
  if (empty(variable_get('spcontactform_sync_type', FALSE))) {
    variable_set('spcontactform_sync_type', 'cron');
  }
  $form['spcontactform_sync_type'] = array(
    '#type' => 'radios',
    '#title' => 'Hoe wilt u contacten synchronizeren?',
    '#default_value' => variable_get('spcontactform_sync_type', array()),
    '#options' => array('cron' => 'Cron', 'direct' => 'Direct'),
    '#required' => TRUE,
  );

  // Max sync items per cron.
  $form['spcontactform_cron_number'] = array(
    '#type' => 'select',
    '#title' => 'Hoeveel webformulier inzendingen moeten per cron gesynchronizeerd worden?',
    '#default_value' => variable_get('spcontactform_cron_number', 0),
    '#options' => range(0, 500, 10),
    '#states' => array(
      'visible' => array(
        ':input[name="spcontactform_sync_type"]' => array('value' => 'cron'),
      ),
    ),
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
  $form['spcontactform_allowed_component_types'] = array(
    '#type' => 'checkboxes',
    '#title' => 'Kies webformulier veld types om te synchronizeren.',
    '#options' => $options,
    '#default_value' => variable_get('spcontactform_allowed_component_types', array()),
  );
  $groups = spcontactform_get_civi_groups();
  $form['spcontactform_hou_hoog_group'] = array(
    '#type' => 'select',
    '#title' => 'Kies de groep van mensen die op de hoogte willen blijven van belangrijk SP nieuws.',
    '#options' => $groups,
    '#empty_option' => '- Geen -',
    '#default_value' => variable_get('spcontactform_hou_hoog_group', array()),
  );
  $tags = spcontactform_get_civi_tags();
  if (empty(variable_get('spcontactform_unverified_tag', FALSE)) && $id = array_search('Ongecontroleerd', $tags)) {
    variable_set('spcontactform_unverified_tag', $id);
  }
  $form['spcontactform_unverified_tag'] = array(
    '#type' => 'select',
    '#title' => 'Kies de tag die moet worden toegevoegd aan contacten die handmatig moeten worden gecontroleerd.',
    '#options' => $tags,
    '#empty_option' => '- Geen -',
    '#default_value' => variable_get('spcontactform_unverified_tag', array()),
  );
  return system_settings_form($form);
}

function spcontactform_group_autocomplete($string) {
  $string = strtolower($string);
  $matches = array();
  if (strlen($string) > 2) {
    $matches = array();
    $groups = spcontactform_get_civi_groups();

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

function spcontactform_overview() {
  $sync_data = variable_get('spcontactform_sync_data', array());
  $postphoned_submission_syncs = variable_get('spcontactform_postphoned_submission_syncs', array());
  $data_names = spcontactform_get_data_names();
  $markup = '<table><tr><th>Webformulier</th><th>Formulier veld &#8594; Civi data</th><th>Civi groepen</th><th>Hou hoog</th><th>Cron wachtrij</th><th></th></tr>';
  foreach ($sync_data as $data) {
    $form = node_load($data['webform']);
    // If form doesn't exist, clear sync data for this form.
    if ($form === FALSE) {
      unset($sync_data[$data['webform']]);
      variable_set('spcontactform_sync_data', $sync_data);
      continue;
    }
    $markup .= '<tr><td><a href="/node/'. $form->nid . '">'.$form->title.'</a></td><td><table>';
    foreach ($data_names as $key => $name) {
      if (!empty($data[$key . '_component'])) {
        $markup .= '<tr><td>' . $form->webform['components'][$data[$key . '_component']]['name'] . '</td><td>&nbsp;&#8594;&nbsp;</td><td>' . $name . '</td></tr>';
      }
    }
    $markup .= '</table></td><td>';
    if (isset($data['selected_groups'])) {
      foreach ($data['selected_groups'] as $groupname) {
        $markup .= $groupname . '<br/>';
      }
    }
    else {
      $markup .= 'Geen';
    }
    if (!empty($data['hou_hoog_component'])) {
      $markup .= '</td><td><table><tr><td>Veld:&nbsp;</td><td>' . $form->webform['components'][$data['hou_hoog_component']]['name'] . '</td></tr><tr><td>Conditie:&nbsp;</td><td>' . $data['hou_hoog_accept_value'] . '</td></tr></table></td>';
    }
    else {
      $markup .= '</td><td>Geen</td>';
    }
    $markup .= '<td>' . count($postphoned_submission_syncs[$form->nid]) . '</td>';
    $markup .= '<td><a href="/admin/config/sp/spcontactform/delete/' . $form->nid . '">verwijder</a></td></tr>';
  }
  $markup .= '</table>';

  return $markup;
}

function spcontactform_delete($id) {
  $sync_data = variable_get('spcontactform_sync_data', array());
  $postphoned_submission_syncs = variable_get('spcontactform_postphoned_submission_syncs', array());
  if (!empty($postphoned_submission_syncs[$id])) {
    unset($postphoned_submission_syncs[$id]);
    variable_set('spcontactform_postphoned_submission_syncs', $postphoned_submission_syncs);
    drupal_set_message('Cron wachtrij contactform nr. ' . $id . ' is verwijderd.');
  }
  if (isset($sync_data[$id])) {
    unset($sync_data[$id]);
    variable_set('spcontactform_sync_data', $sync_data);
    drupal_set_message('SP contactform nr. ' . $id . ' is verwijderd.');
  }
  else {
    drupal_set_message('SP contactform nr. ' . $id . ' kon niet worden verwijderd.', 'error');
  }
  drupal_goto('/admin/config/sp/spcontactform');
}
