<?php

// Implements hook_drush_command
function spwebformsync_drush_command() {
  $items = array();

  $items['spwebformsync-retry'] = array(
    'description' => "Retry syncs that gave error.",
    'aliases'     => array('spwsr'),
  );

  $items['spwebformsync-sync'] = array(
    'description' => "Sync.",
    'aliases'     => array('spws'),
  );

  return $items;
}

/*
 * Syncs not yet synced webform submissions.
 */
function drush_spwebformsync_sync() {
  $sync_type = variable_get('spwebformsync_sync_type');
  if ($sync_type == 'cron') {
    set_time_limit(120);
    watchdog('spwebformsync', 'Sync started by drush.');
    spwebformsync_sync();
    watchdog('spwebformsync', 'Sync ended.');
  }
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
