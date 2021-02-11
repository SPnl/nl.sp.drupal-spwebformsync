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

function spwebformsync_get_webform_allowed_components($nid, $allowed_types = NULL) {
  // Set default allowed types.
  if (empty($allowed_types)) {
    $allowed_types = variable_get('spwebformsync_allowed_component_types', array());
    // Always allow spcontact component.
    if (module_exists('spcontact')) {
      $allowed_types['spcontact'] = 'spcontact';
    }
  }
  // Get components.
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

function spwebformsync_get_groups($sync = TRUE) {
  $selections = procapi_selection_get_select_list();
  return $selections;
}

function spwebformsync_parse_webform_select_options($webform_id, $selected_component) {
  if (empty($webform_id)) {
    return array();
  }
  $webform = node_load($webform_id);
  $options = array();
  switch($webform->webform['components'][$selected_component]['type']) {
    case 'select':
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
      break;

    case 'spcontact':
      if (!empty($webform->webform['components'][$selected_component]['extra']['optional_questions'])) {
        $items = $webform->webform['components'][$selected_component]['extra']['optional_questions'];
        foreach ($items as $item) {
          if (!empty($item)) {
            $spcontact_items = spcontact_select_options($webform->webform['components'][$selected_component]);
            $options[$item] = $spcontact_items[$item];
          }
        }
      }
      break;
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

function spwebformsync_get_components_list($webform_components, $type = 'normal') {
  $list = array();
  foreach ($webform_components as $component_id => $component) {
    switch ($component['type']) {
      case 'sp_address_auto':
        // Replace with fields.
        $elements = webform_component_invoke('sp_address_auto', 'render', $component);
        foreach ($elements as $element_id => $element) {
          if (strpos($element_id, '#') === 0) {
            continue;
          }
          $list[$component_id . '|' . $element_id] = $component['name'] . ': ' . $element['#title'];
        }
        break;

      case 'spaddresscomp':
        // Replace with fields.
        $elements = webform_component_invoke('spaddresscomp', 'render', $component);
        foreach ($elements as $element_id => $element) {
          if (strpos($element_id, '#') === 0) {
            continue;
          }
          $list[$component_id . '|' . $element_id] = $component['name'] . ': ' . $element['#title'];
        }
        break;

      case 'spcontact':
        if ($type == 'option_components_only') {
          $list[$component_id] = substr($component['name'], 0, 40);
        }
        else {
          $questions = spcontact_text_questions();
          $no = 0;
          foreach ($questions as $key => $question) {
            $list[$component_id . '|' . $no] = 'SP Contact: ' . $question;
            $no++;
          }
        }
        break;

      default:
        $list[$component_id] = substr($component['name'], 0, 40);
        break;
    }
  }
  return $list;
}

function spwebformsync_get_tokens_text($webform_id) {
  $tokens_text = '';
  if (!empty($webform_id)) {
    $webform = node_load($webform_id);
    $tokens_text = '';
    $tokens = spwebformsync_get_component_tokens($webform->webform['components']);
    if (!empty($tokens)) {
      $tokens_text .= 'Er zijn de volgende tokens beschikbaar die worden vervangen door de gebruiker ingevulde waarden: ';
      $tokens_text .= join(' en ', array_filter(array_merge(array(join(', ', array_slice($tokens, 0, -1))), array_slice($tokens, -1)), 'strlen'));
      $tokens_text .= '.';
    }
  }
  return $tokens_text;
}
