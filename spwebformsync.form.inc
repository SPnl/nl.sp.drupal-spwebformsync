<?php

/* ---------------------------------------------------------------------------
 * FORM RELATED FUNCTIONS
 * ---------------------------------------------------------------------------
 */

function spwebformsync_get_webforms() {
  $webforms = &drupal_static(__FUNCTION__);
  if (!isset($webforms)) {
    $webforms = array();
    $query = new EntityFieldQuery();

    $result = $query
      ->entityCondition('entity_type', 'node')
      ->entityCondition('bundle', 'webform')
      ->execute();

    if (!empty($result['node'])) {
      $nids = array_keys($result['node']);
      $webforms = entity_load('node', $nids);
    }
  }
  // Filter already set forms.
  $sync_data = variable_get('spwebformsync_sync_data', array());
  foreach ($webforms as $key => $webform) {
    if (in_array($webform->nid, array_keys($sync_data))) {
      unset($webforms[$key]);
    }
  }
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
        if (_spcf_check_result($groups_results, 'groepen', 'get')) {
          if (!empty($groups_results['values'])) {
            foreach ($groups_results['values'] as $value) {
              if (
                empty($value['group_type']) ||
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

function spwebformsync_get_civi_tags($sync = TRUE) {
  if ($sync) {
    $tags = &drupal_static(__FUNCTION__);
    if (!isset($tags)) {

      if ($cache = cache_get('spwebformsync_civi_tags')) {
        $tags = $cache->data;
      }
      else {
        $tags = array();
        set_time_limit(0);
        $spcivi = \SPCivi::getInstance();
        $params = array(
          'sequential'            => 1,
          'option.limit'          => 9999,
          'is_selectable' => 1,
          'used_for' => 'civicrm_contact',
        );
        $tags_results = $spcivi->api('Tag', 'get', $params);
        if (_spcf_check_result($tags_results, 'tags', 'get')) {
          if (!empty($tags_results['values'])) {
            foreach ($tags_results['values'] as $value) {
              $tags[$value['id']] = $value['name'];
            }
            asort($tags);
            variable_set('spwebformsync_civi_tags', $tags);
          }
          else {
            drupal_set_message('Error syncing civicrm tags, using saved data.', 'warning');
          }
        }
      } 
      $tags = variable_get('spwebformsync_civi_tags', array());
      cache_set('spwebformsync_civi_tags', $tags, 'cache', time() + 3600);
    }
    return $tags;
  }
  $tags = variable_get('spwebformsync_civi_tags', array());
  return $tags;
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
