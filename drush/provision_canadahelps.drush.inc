<?php

require_once(dirname(__FILE__) . '/verify.provision.inc');

/**
 * Implementation of hook_drush_command().
 */
function provision_canadahelps_drush_command() {
  $items = [];
  $items['provision-canadahelps-site-key-reset'] = [
    'description' => 'Reset Site key for a site',
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
  ];
  return $items;
}

function provision_canadahelps_hosting_site_context_options(&$task) {
  $d9_id = db_query('SELECT dms_instance_id FROM hosting_site WHERE nid = :nid LIMIT 1', [':nid' => $task->ref->nid])->fetchField();
  $task->context_options['hosting_canadahelps_d9_id'] = $d9_id ?? '';
  drush_log('provision_canadahelps: d9_instance_id = ' . $d9_id, 'ok');
}

/**
 * Implements hook_drush_init().
 */
function provision_canadahelps_drush_init() {
  // Register our service classes for autoloading.
  provision_canadahelps_provision_register_autoload();
}

/**
 * Register our directory as a place to find Provision classes.
 *
 * This allows Provision to autoload our classes, so that we don't need to
 * specifically include the files before we use the class.
 */
function provision_canadahelps_provision_register_autoload() {
  static $loaded = FALSE;
  if (!$loaded) {
    $loaded = TRUE;
    $list = drush_commandfile_list();
    $provision_dir = dirname($list['provision']);
    include_once($provision_dir . '/provision.inc');
    include_once($provision_dir . '/provision.service.inc');
    provision_autoload_register_prefix('Provision_', dirname(__FILE__));
  }
}

/**
 * Expose the service type this extension defines to provision.
 *
 * @return
 *   An array with the service type the key, and the default implementation the value.
 */
function provision_canadahelps_provision_services() {
  provision_canadahelps_provision_register_autoload();
  return [
    'canadahelps' => NULL,
  ];
}

function drush_provision_canadahelps_site_key_reset() {
  drush_bootstrap(DRUSH_BOOTSTRAP_DRUPAL_FULL);
  $options = array();
  $target = d()->uri;
  provision_backend_invoke($target, 'provision-verify', array(), $options);
  drush_log('Site Key being reset', 'ok');
}
