<?php

// Implements hook_drush_command
function spwebformsync_drush_command() {
  $items = array();

  $items['spwebformsync-retry'] = array(
    'description' => "Retry syncs that gave error.",
    'aliases'     => array('spwsr'),
  );

  return $items;
}

function drush_spwebformsync_retry() {
 $num_updated = db_update('spwebformsync_syncstate')
  ->fields(array(
    'state' => 0,
  ))
  ->condition('state', 2)
  ->execute(); 
  watchdog('spwebformsync', 'Reset ' . $num_updated . ' syncs that gave an error. They will be retried on next cron run.');
  drupal_set_message('Reset ' . $num_updated . ' syncs that gave an error. They will be retried on next cron run.');
}
