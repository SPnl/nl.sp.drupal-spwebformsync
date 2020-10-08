<?php

/**
 * Get oauth2 client token.
 */
function procpush_get_oauth2_client_token() {
  $server_domain = variable_get('procpush_procurios_server_domain');
  $client_id = variable_get('procpush_oauth_client_id');
  $client_secret = variable_get('procpush_oauth_client_secret');
  $scope = variable_get('procpush_oauth_scope', '');

  if (
    !empty($server_domain) &&
    !empty($client_id) &&
    !empty($client_secret) &&
    !empty($scope)
  ) {
    $oauth2_config = array(
      'token_endpoint' => 'https://' . $server_domain . PROCPUSH_OAUTH_TOKEN_PATH,
      'auth_flow' => 'client-credentials',
      'client_id' => $client_id,
      'client_secret' => $client_secret,
      'scope' => $scope,
    );

    try {
      $oauth2_client = new OAuth2\ProcClient($oauth2_config, $client_id);
      $access_token = $oauth2_client->getAccessToken();
    }
    catch (Exception $e) {
      drupal_set_message($e->getMessage(), 'error');
    } 
    if (!empty($access_token)) {
      return $access_token;
    }
  }
  return FALSE;
}

/**
 * Flatten properties.
 */
function procpush_flatten_properties($properties, &$fields, &$key, &$title) {
  foreach ($properties as $prop_key => $property) {
    if (in_array($property['type'], array('string', 'integer', 'object'))) {
      if ($property['type'] == 'object' && isset($property['properties'])) {
        $key = $key . $prop_key . '|';
        $title = $title . (empty($property['title']) ? $prop_key : $property['title']) . '|';
        procpush_flatten_properties($property['properties'], $fields, $key, $title);
      }
      else {
        $fields[$key . $prop_key] = $title . (empty($property['title']) ? $prop_key : $property['title']);
      }
    }
  }
  $key = preg_replace('#[^|]*\|$#', '', $key);
  $title = preg_replace('#[^|]*\|$#', '', $title);
}

/**
 * Get form schema fields
 */
function procpush_get_proc_form_schema_contact_fields($form_id) {
  $schema = procpush_get_proc_form_schema($form_id);
  $properties = $schema[$form_id]['properties'];
  $raw_fields = array();
  $key = '';
  $title = '';
  procpush_flatten_properties($properties, $raw_fields, $key, $title);
  foreach($raw_fields as $key => $title) {
    $fields[$key] = str_replace('|', ' - ', $title);
  }
  return $fields;
}

/**
 * Get api form schema.
 */
function procpush_get_proc_form_schema($form_id) {
  $schema = &drupal_static(__FUNCTION__);
  if (!isset($schema[$form_id])) {
    $schema[$form_id] = NULL;
    if ($cache = cache_get('procpush_schema_' . $form_id)) {
      $schema[$form_id] = $cache->data;
    }
    else {
      // Get registration set.
      $registration_set = procpush_get_proc_registration_set();
      $path = $registration_set[$form_id]['_links']['schema']['href'];
      $method = $registration_set[$form_id]['_links']['schema']['type'];
      $result = procpush_perform_request($path, $method, $error);
      if (!empty($result)) {
        if (is_array($result) && !empty($result['title'])) {
          $schema[$form_id] = $result;
          cache_set('procpush_schema_' . $form_id, $schema[$form_id], 'cache', time() + 360);
        }
      }
    }
  }
  return $schema;
}

/**
 * Validate contact api object.
 */
function procpush_proc_validate_object($object, $form_id, &$error) {
  // Get registration set.
  $registration_set = procpush_get_proc_registration_set();
  $path = $registration_set[$form_id]['_links']['validate']['href'];
  $method = $registration_set[$form_id]['_links']['validate']['type'];
  $result = procpush_perform_request($path, $method, $error, NULL, json_encode($object));
  return $result;
}

/**
 * Push contact api object.
 */
function procpush_proc_push_object($object, $form_id, &$error) {
  // Get registration set.
  $registration_set = procpush_get_proc_registration_set();
  $path = $registration_set[$form_id]['_links']['registration']['href'];
  $method = $registration_set[$form_id]['_links']['registration']['type'];
  $result = procpush_perform_request($path, $method, $error, NULL, json_encode($object));
  return $result;
}

/**
 * Get registration set list.
 */
function procpush_get_proc_forms_list() {
  $list = &drupal_static(__FUNCTION__);
  if (!isset($list)) {
    $list = array();
    $registration_set = procpush_get_proc_registration_set();
    foreach ($registration_set as $registration_form) {
      $list[$registration_form['id']] = $registration_form['title'];
    }
  }
  return $list;
}

/**
 * Get registration set.
 */
function procpush_get_proc_registration_set() {
  $registration_set = &drupal_static(__FUNCTION__);
  if (!isset($registration_set)) {
    $registration_set = array();
    if ($cache = cache_get('procpush_registration_set')) {
      $registration_set = $cache->data;
    }
    else {
      $token = procpush_get_oauth2_client_token();
      if (!empty($token)) {
        // Get registration set.
        $server_domain = variable_get('procpush_procurios_server_domain');
        $path = PROCPUSH_API_REGISTRATION_SET_PATH;
        $result = procpush_perform_request($path, 'GET', $error);
        if (!empty($result)) {
          if (!empty($result['registrationSet'])) {
            foreach($result['registrationSet'] as $registration_form) {
              $registration_set[$registration_form['id']] = $registration_form;
            }
            cache_set('procpush_registration_set', $registration_set, 'cache', time() + 360);
          }
        }
      }
    }
  }
  return $registration_set;
}

/**
 * Perform api request.
 */
function procpush_perform_request($path, $method, &$error, $headers = NULL, $data = NULL) {
  if (empty($headers)) {
    $token = procpush_get_oauth2_client_token(); 
    $headers = array(
      'Content-Type' => 'application/json',
      'Accept' => 'application/vnd.procurios.profile+json; version=1',
      'Accept-Language' => 'en-US,en;q=0.5',
      'Authorization' => 'Bearer ' . $token,
    );
  }
  $options = array(
    'method' => $method,
    'headers' => $headers,
  );
  if (!empty($data)) {
    $options['data'] = $data;
  }
  $server_domain = variable_get('procpush_procurios_server_domain');
  $url = 'https://' . $server_domain . $path;

  $result = drupal_http_request($url, $options);

  if ($result->code != 200) {
    $error = $result;
    return FALSE;
  }

  
  if (procpush_is_json($result->data)) {
    $response = drupal_json_decode($result->data);
  }
  else {
    $response = $result->data;
  }
  return $response;
}

/**
 * Check if string is json.
 */
function procpush_is_json($string) {
  json_decode($string);
  return (json_last_error() == JSON_ERROR_NONE);
}

/**
 * Create procurios form submission object from webformsync data.
 */
function procpush_create_data_object($data) {
  $mapping = variable_get('procpush_field_mapping');
  $object = array();
  foreach ($mapping as $webformsync_key => $procurios_key) {
    $keys = explode('|', $procurios_key);
    if (!empty($data[$webformsync_key])) {
      $md = array();
      $md[$keys[count($keys)-1]] = $data[$webformsync_key];
      for($i=count($keys)-2; $i>-1; $i--) {
        $md[$keys[$i]] = $md;
        unset($md[$keys[$i+1]]);
      }
      $object = array_merge_recursive($object, $md);
    }
  }
  return $object;
}

/**
 * Checks if the address data is sufficient.
 */
function procpush_check_address_sufficient($data) {
  // Check for Testlaan (street) 1 (house_number), Amsterdam (locality)
  if (!empty($data['street']) && !empty($data['house_number']) && !empty($data['locality'])) {
    return TRUE;
  }
  // Check for house number and postal code, for example: 1234AX 7
  // This is sufficient because we can determine the street and city
  // from the postal code.
  if (!empty($data['house_number']) && !empty($data['postal_code'])) {
    return TRUE;
  }
  return FALSE;
}

/**
 * Checks wether a submission contains sufficient data to create new or sync existing civi contact.
 */
function procpush_check_submission_sufficient($data) {
  // Check if values are sufficient to create new or sync existing civi contact.
  if (!empty($data['contact_id'])) {
    return TRUE;
  }

  if (
    !empty($data['first_name']) ||
    !empty($data['last_name'])||
    !empty($data['name'])
  ) {
    if (!empty($data['email']) || !empty($data['telephone']) || procpush_check_address_sufficient($data)) {
      return TRUE;
    }
  }
  return FALSE;
}

/*
 * Mail error to tdgraaff.
 */
function procpush_error($error_message, $debug_data) {
  global $base_url;
  $message = array(
    'content' => '<p>' . $error_message . '</p><p><pre>Debug data:<pre>@message_data</pre></p>',
    'data' => array(
      '@message_data' => print_r($debug_data, TRUE),
    ),
  );
  watchdog('procpush', $message['content'], $message['data'], WATCHDOG_ERROR);
  drupal_mail('spwebformsync', 'error', 'tdgraaff@sp.nl', language_default(), $message, 'webmaster@sp.nl');
}
