<?php

/* ---------------------------------------------------------------------------
 * FORM RELATED FUNCTIONS
 * ---------------------------------------------------------------------------
 */

function spwebformsync_get_webforms() {
  $webforms = &drupal_static(__FUNCTION__);
  if (!isset($webforms)) {
    $webform_node_types = webform_variable_get('webform_node_types');
    $webforms = array();
    $query = new EntityFieldQuery();

    $result = $query
      ->entityCondition('entity_type', 'node')
      ->entityCondition('bundle', $webform_node_types, 'IN')
      ->execute();

    if (!empty($result['node'])) {
      $nids = array_keys($result['node']);
      $webforms = entity_load('node', $nids);
    }
  }
  /*
  // Filter already set forms.
  $sync_data = variable_get('spwebformsync_sync_data', array());
  foreach ($webforms as $key => $webform) {
    if (in_array($webform->nid, array_keys($sync_data))) {
      unset($webforms[$key]);
    }
  }
  */
  return $webforms;
}

function spwebformsync_get_webform_allowed_components($nid, $allowed_types) {
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

function spwebformsync_get_civi_groups($sync = TRUE) {
  if ($sync) {
    $groups = &drupal_static(__FUNCTION__);
    if (!isset($groups)) {

      if ($cache = cache_get('spwebformsync_civi_groups')) {
        $groups = $cache->data;
      }
      else {
        $groups = array();
        set_time_limit(0);
        $spcivi = \SPCivi::getInstance();
        $params = array(
          'sequential'            => 1,
          'option.limit'          => 9999,
        );
        $groups_results = $spcivi->api('Group', 'get', $params);
        if (_spcf_check_result($groups_results, 'groepen', 'get', $params)) {
          if (!empty($groups_results['values'])) {
            foreach ($groups_results['values'] as $value) {
              if (
                empty($value['group_type']) ||
                (array_key_exists(0, $value['group_type']) && empty($value['group_type'][0])) ||
                in_array('2', $value['group_type'])
              ) {
              $groups[$value['id']] = $value['title'];
              }
            }
            asort($groups);
            variable_set('spwebformsync_civi_groups', $groups);
          }
          else {
            drupal_set_message('Error syncing civicrm groups, using saved data.', 'warning');
          }
        }
      } 
      $groups = variable_get('spwebformsync_civi_groups', array());
      cache_set('spwebformsync_civi_groups', $groups, 'cache', time() + 3600);
    }
    return $groups;
  }
  $groups = variable_get('spwebformsync_civi_groups', array());
  return $groups;
}

function spwebformsync_parse_webform_select_options($webform, $selected_component) {
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

function spwebformsync_parse_group_form_select_value($group_raw) {
  $group = '';
  preg_match('@^.+\(([0-9]+)\)$@', $group_raw, $matches);
  if (!empty($matches[1])) {
    $group = $matches[1];
  }
  return $group;
}

function spwebformsync_get_components_list($webform_components) {
  $list = array();
  foreach ($webform_components as $component_id => $component) {
    if ($component['type'] === 'sp_address_auto') {
      // Replace with fields.
      $elements = webform_component_invoke('sp_address_auto', 'render', $component);
      foreach ($elements as $element_id => $element) {
        if (strpos($element_id, '#') === 0) continue;
        $list[$component_id . '|' . $element_id] = $component['name'] . ': ' . $element['#title'];
      }
    }
    else {
      $list[$component_id] = substr($component['name'], 0, 40);
    }
  }
  return $list;
}

function spwebformsync_filter_token_components($components) {
  $filtered_components = array();
  $exclude = array('valid_test', 'call_answered', 'target_available', 'not_available_today', 'call_appointment', 'caller_message');
  foreach ($components as $id => $component) {
    if (strpos($component['form_key'], 'spbellen_') === false && in_array($component['type'], array('textfield', 'textarea', 'select')) && !in_array($component['form_key'], $exclude)) {
      $filtered_components[$id] = $component;
    }
  }
  return $filtered_components;
}

function spwebformsync_get_component_tokens($components) {
  $tokens = array();
  $filtered_components = spwebformsync_filter_token_components($components);
  if (!empty($filtered_components)) {
    foreach ($filtered_components as $component) {
      $tokens[] = '[' . $component['form_key'] . ']';
    }
  }
  return $tokens;
}

function spwebformsync_get_tokens_text($components) {
  $tokens_text = '<strong>Let op!</strong> Het is mogelijk om CiviCRM contact veldwaarden in de mail en het onderwerp te gebruiken door de veldnaam tussen vierkante haken te plaatsen. Onder andere [contact_id], [first_name], [middle_name], [last_name], [display_name], [email], [phone], [mobile], [afdeling] en [afdeling_code].';
  $tokens = spwebformsync_get_component_tokens($components);
  if (!empty($tokens)) {
    $tokens_text .= '<br/>Daarnaast zijn er de volgende tokens die worden vervangen door de ingevulde veldwaarden: ';
    $tokens_text .= join(' en ', array_filter(array_merge(array(join(', ', array_slice($tokens, 0, -1))), array_slice($tokens, -1)), 'strlen'));
    $tokens_text .= '.';
  }
  return $tokens_text;
}
