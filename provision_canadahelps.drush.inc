<?php

function provision_canadahelps_site_context_options(&$task) {
  $node = hosting_context_load($task->ref->hosting_name);
  $d9_id = db_query('SELECT dms_instance_id FROM hosting_site WHERE nid = :nid ORDER BY created DESC LIMIT 1', [':nid' => $node->id])->fetchField();
  $task->context_options['hosting_canadahelps_d9_id'] = $d9_id;
}

/**
 * @todo Please document this function.
 * @see http://drupal.org/node/1354
 */
function provision_canadahelps_post_hosting_clone_task($task, $data) {
  if ($task->ref->type == 'site') {
    if (!empty($task->task_args['dms_instance_id'])) {
      db_query('UPDATE {hosting_site} SET dms_instance_id = :dms_instance_id WHERE title = :title', [':dms_instance_id' => $task->task_args['dms_instance_id'], ':title' => hosting_site_clean_domain($task->task_args['new_uri'])]);
    }
  }
}

/**
 * Implements the provision-symbiotic-login command.
 *
 * Mostly copies /usr/share/drush/commands/provision/platform/reset.login.provision.inc
 * but for uid = 2.
 */
function drush_provision_canadahelps_site_key_reset() {
  drush_log(dt('Canadahelps Provision Site Key reset task'), 'success');
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
    provision_autoload_register_prefix('Provision_', dirname(__FILE__));
  }
}

/**
 * Expose the service type this extension defines to provision.
 *
 * @return
 *   An array with the service type the key, and the default implementation the value.
 */
function provision_civicrm_provision_services() {
  provision_canadahelps_provision_register_autoload();
  return [
    'canadahelps' => NULL,
  ];
}

