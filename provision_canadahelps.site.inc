<?php
/**
 * @file
 *   Hosting REST API functions, and Drupal hooks.
 */

/**
 * Implements the 'site' API.
 */
function provision_canadahelps_site() {
  provision_canadahelps_cors_headers();

  $method = strtolower($_SERVER['REQUEST_METHOD']);
  $f = 'provision_canadahelps_site_' . $method;

  try {
    if (function_exists($f)) {
      $result = $f();
    }
    else {
      $result = array(
        'status' => 'error',
        'message' => 'Unknown method for site: ' . $_SERVER['REQUEST_METHOD'],
      );
    }
  }
  catch (Exception $e) {
    $result = array(
      'status' => 'error',
      'message' => $e->getMessage(),
    );
  }

  echo json_encode($result);
  drupal_exit();
}

/**
 * Implements the 'site GET' API (get info/status).
 */
function provision_canadahelps_site_get() {
  // FIXME: we want the user to have live updates on the status of the site,
  // so either we grant some temporary access, or we grant to anons.

  $access = provision_canadahelps_check_access($_GET['key'], $_GET['secret']);
  if (!$access) {
    throw new Exception(t('Access denied.'));
  }

  $url = $_GET['url'];
  $dms_instance_id = (isset($_GET['dms_instance_id']) ? $_GET['dms_instance_id'] : NULL);

  if (! $url) {
    throw new Exception(t('The "url" parameter was empty.'));
  }

  // If we had the invoice_id, return the site status
  // NB: as a security measure, the invoice_id and url must match.
  if ($dms_instance_id) {
    $order_id = db_query('SELECT id FROM provision_canadahelps_order WHERE dms_instance_id = :dms_instance_id AND site = :url', array(':dms_instance_id' => $dms_instance_id, ':url' => $url))->fetchField();
    $site = hosting_get_site_by_url($url);

    if (! $order_id) {
      throw new Exception(t('Invalid invoice ID or URL. Please contact support for more information.'));
    }

    // Fetch the last log.
    $result = db_query('SELECT * FROM provision_canadahelps_log WHERE order_id = :order_id ORDER BY created DESC limit 1', array(':order_id' => $order_id));

    if ($record = $result->fetchObject()) {
      // If it's a new order, check if the cloning has begun.
      if ($record->task == PROVISION_CANADAHELPS_ORDER_STATUS_NEW) {
        // FIXME HARDCODE: 608 = clone task of demo.s.c
        $vid = db_query("SELECT max(vid) from hosting_task_arguments where nid = 608 and name = 'new_uri' and value = :url", array(':url' => $url))->fetchField();
        $clone_status = db_query("SELECT task_status FROM hosting_task WHERE vid = :vid", array(':vid' => $vid))->fetchField();

        if ($clone_status == HOSTING_TASK_PROCESSING) {
          $result = array(
            'status' => 'success',
            'data' => array(
              'site_url' => $site->title,
              'site_status' => PROVISION_CANADAHELPS_ORDER_STATUS_CLONE_INPROGRESS,
            ),
          );
          return $result;
        }
        elseif ($clone_status == HOSTING_TASK_ERROR) {
          $result = array(
            'status' => 'error',
            'data' => array(
              'site_url' => $site->title,
              'message' => "Clone operation failed.",
            ),
          );
          return $result;
        }
      }
      elseif ($record->task == PROVISION_CANADAHELPS_ORDER_STATUS_VERIFY_COMPLETE) {
        $result = array(
          'status' => 'success',
          'data' => array(
            'site_url' => $site->title,
            'site_status' => $record->task,
            'login_link' => 'https://' . $site->title . '/user',
          ),
        );

        return $result;
      }

      $result = array(
        'status' => 'success',
        'data' => array(
          'site_url' => $site->title,
          'site_status' => $record->task,
        ),
      );
      return $result;
    }
  }

  // Otherwise just return if the site exists or not.
  $site = hosting_get_site_by_url($url);

  if (! $site) {
    throw new Exception(t('Site not found. Please contact support for more information.'));
  }

  $result = array(
    'status' => 'success',
    'data' => array(
      'site_url' => $site->title,
      'site_status' => $site->site_status,
    ),
  );

  return $result;
}

/**
 * Implements the 'site POST' API (create).
 *
 *
 * The main domain ([url].example.org) is now enforced by hosting_saas, so
 * we only handle the [url] is rather the subdomain.
 */
function provision_canadahelps_site_post() {
  $access = provision_canadahelps_check_access($_POST['key'], $_POST['secret']);

  if (!$access) {
    throw new Exception(t('Access denied.'));
  }

  // TODO : check if URL format is OK (i.e. no spaces, etc)
  $url = check_plain($_POST['url']);
  $url = preg_replace('/[^\.a-z0-9]/', '', $url);

  if ($url != $_POST['url']) {
    throw new Exception('The "url" parameter had invalid characters.');
  }

  _provision_canadahelps_is_allowed_creation($url);

  $dms_instance_id = $_POST['dms_instance_id'];

  // Log order in provision_canadahelps_order.
  $record = new StdClass();
  $record->dms_instance_id = $dms_instance_id;
  $record->site = $url;
  $record->token = sha1($url . uniqid(mt_rand()));
  $record->ip = $_SERVER['REMOTE_ADDR'];
  $record->current_status = PROVISION_CANADAHELPS_ORDER_STATUS_NEW;
  $record->created = time();
  $record->updated = time();

  drupal_write_record('provision_canahelps_order', $record);

  // Clone a site
  $template_nid = variable_get('provision_canadahelps_template_site_nid', NULL);
  $new_platform_nid = variable_get('provision_canadahelps_target_platform', NULL);
  $new_db_server = variable_get('provision_canadahelps_db_server', 2); // 2 = DB server on single-server default setups

  if (provision_canadahelps_check_capacity() === FALSE) {
    throw new Exception('Service has reached max capacity.');
  }

  $result = hosting_add_task($template_nid, 'clone', [
    'new_uri' => $url,
    // 2 is the default nid for single-server setups
    'new_db_server' => $new_db_server,
    'target_platform' => $new_platform_nid,
    'aliases' => '',
    'redirection' => '',
    'https_enabled' => 2, // FIXME : Use template site's setting?
    'dms_instance_id' => $dms_instance_id,
  ]);

  provision_canadahelps_log($url, PROVISION_CANADAHELPS_ORDER_STATUS_NEW, t('The order is valid and has been created.'));
  return array('status' => 'success', 'data' => $result);
}

/**
 * A few quick pre-flight checks to make sure that the system
 * is correctly configured (Drupal variales) and that the URL
 * is valid.
 */
function _provision_canadahelps_is_allowed_creation($url) {
  if (!$url) {
    throw new Exception('The "url" parameter was empty.');
  }

  // This function checks for uniqueness of domains and aliases.
  // TODO: we should also implement hook_allow_domain() to enforce domain?
  if (! hosting_domain_allowed($url)) {
    throw new Exception('The url is not allowed by local configurations: ' . $url);
  }

  $site = hosting_get_site_by_url($url);

  if ($site) {
    throw new Exception('The site already exists.');
  }

  $variables = array(
    'provision_canadahelps_target_platform',
    'provision_canadahelps_db_server',
    'provision_canadahelps_template_site_nid',
  );

  foreach ($variables as $to_check) {
    if (variable_get($to_check, NULL) === NULL) {
      throw new Exception("Variable hasn't been chosen yet: " . $to_check);
    }
  }

  return $site;
}

/**
 * Checks if an invoice ID is valid (not already used, and valid in the transactional site).
 * i.e. checks if the invoice_id is valid in CiviCRM.
 *
 * @param String $dms_instance_id
 * @returns Boolean
 */
function hosting_restapi_is_valid_dms($dms_instance_id) {
  if (! $dms_instance_id) {
    throw new Exception('Missing invoice.');
  }

  // Check if the invoice_id was already used.
  // XXX assumes that 1 invoice = 1 site. We don't support multi-quota stuff,
  // since we don't give access to Aegir.
  $exists = db_query('SELECT count(*) as cpt FROM provision_canadahelps_order WHERE dms_instance_id = :dms_instance_id', array(':dms_instance_id' => $dms_instance_id))->fetchField();

  if ($exists) {
    watchdog('provision_canadahelps', 'DMS ID already used.');
    throw new Exception('Invalid DMS ID.');
  }

  // NB: for now, we assume that hook implementations will throw an exception
  // if they couldn't validate the dms_instance_id. It assumes we have only one
  // way to validate invoices (create orders). If we want to change this at
  // some point, we should add a "invoice_type" argument.
  module_invoke_all("provision_canadahelps_validate_dms_instance_id", $dms_instance_id);

  return TRUE;
}