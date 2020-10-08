<?php

/**
 * Settings form.
 */
function procpush_settings_form($form, &$form_state) {
  $values = isset($form_state['values']) ? $form_state['values'] : array();
  $storage = &$form_state['storage'];


  $form['#prefix'] = '<div id="procpush_ajax_form">';
  $form['#suffix'] = '</div>';

  $form['procpush_procurios_server_domain'] = array(
    '#type' => 'textfield',
    '#title' => 'Procurios server domain',
    '#default_value' => variable_get('procpush_procurios_server_domain', ''),
    '#required' => TRUE,
  );

  $form['procpush_oauth_client_id'] = array(
    '#type' => 'textfield',
    '#title' => 'Oauth client id',
    '#default_value' => variable_get('procpush_oauth_client_id', ''),
    '#required' => TRUE,
  );

  $form['procpush_oauth_client_secret'] = array(
    '#type' => 'textfield',
    '#title' => 'Oauth client secret',
    '#default_value' => variable_get('procpush_oauth_client_secret', ''),
    '#required' => TRUE,
  );

  $form['procpush_oauth_scope'] = array(
    '#type' => 'textfield',
    '#title' => 'Oauth scope',
    '#default_value' => variable_get('procpush_oauth_scope', ''),
    '#required' => TRUE,
  );

  foreach (array(
    'procpush_procurios_server_domain',
    'procpush_oauth_client_id',
    'procpush_oauth_client_secret',
    'procpush_oauth_scope',
  ) as $key) {
    if (!empty(variable_get($key))) {
      $form[$key]['#attributes']['disabled'] = TRUE;
    }
  }


  // Get target forms from procurios.
  $api_forms_list = procpush_get_proc_forms_list();
  $form['procpush_api_form'] = array(
    '#type' => 'select',
    '#title' => 'Kies het procurios API formulier',
    '#name' => 'procpush_api_form',
    '#options' => $api_forms_list,
    '#default_value' => variable_get('procpush_api_form'),
    '#required' => TRUE,
    '#ajax' => array(
      'wrapper' => 'procpush_ajax_form',
      'callback' => 'procpush_ajax_form',
    ),
  );

  $form_id = variable_get('procpush_api_form', '');
  if (!empty($form_id)) {
    $form['field_mapping'] = array(
      '#type' => 'fieldset',
      '#title' => 'Veld mapping',
    );
    $webformsync_fields = spwebformsync_contact_data_fields();
    $procurios_fields = procpush_get_proc_form_schema_contact_fields($form_id);
    foreach ($webformsync_fields as $webformsync_field_key => $webformsync_field_title) {
      $form['field_mapping']['procpush_field_' . $webformsync_field_key . '_mapping'] = array(
        '#type' => 'select',
        '#title' => $webformsync_field_title,
        '#options' => $procurios_fields,
        '#empty_option' => '- selecteer -',
        '#default_value' => variable_get('procpush_field_' . $webformsync_field_key . '_mapping', ''),
      );
    }
  }

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => 'Opslaan',
    '#name' => 'submit',
  );

  return $form;
}

/**
 * Validate settings form.
 */
function procpush_settings_form_validate($form, &$form_state) {
  $values = &$form_state['values'];
  $storage = &$form_state['storage'];

  // Validate domain.
  $values['procpush_procurios_server_domain'] = preg_replace('#^https://#', '', $values['procpush_procurios_server_domain']);
  if (!filter_var('https://' . $values['procpush_procurios_server_domain'], FILTER_VALIDATE_URL)) {
    form_set_error('procpush_procurios_server_url', 'Set a valid domain (https://domain.nl).');
  }
  else {
    $url_parts = parse_url('https://' . $values['procpush_procurios_server_domain']);
    $values['procpush_procurios_server_domain'] = $url_parts['host'];
  }

  // Store values.
  foreach (array(
    'procpush_procurios_server_domain',
    'procpush_oauth_client_id',
    'procpush_oauth_client_secret',
    'procpush_oauth_scope',
    'procpush_api_form',
  ) as $form_key) {
    if (!empty($values[$form_key])) {
      variable_set($form_key, $values[$form_key]);
    }
  }

  // Process ajax.
  if (!empty($form_state['triggering_element']['#name'])) {
    // Check fixed name buttons.
    $trigger = $form_state['triggering_element']['#name'];
    switch ($trigger) {
      case 'procpush_api_form':
        break;
    }
  }
}

/**
 * Submit settings form.
 */
function procpush_settings_form_submit($form, &$form_state) {
  $values = &$form_state['values'];
  $storage = &$form_state['storage'];

  $mapping = array();
  foreach ($values as $key => $value) {
    if (strpos($key, 'procpush_') !== FALSE) {
      preg_match('#^procpush_field_(.*)_mapping$#', $key, $matches);
      if (!empty($matches[1]) && !empty($value)) {
        $mapping[$matches[1]] = $value;
      }
      variable_set($key, $value);
    }
  }
  variable_set('procpush_field_mapping', $mapping);
}

/**
 * Ajax return form.
 */
function procpush_ajax_form($form, &$form_state) {
 return $form;
}
