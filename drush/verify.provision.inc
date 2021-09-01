<?php
require_once(dirname(__FILE__) . '/../provision_canadahelps.variables.php');

/**
 * Login to D9 Instance using token
 */
function login_to_skynet() {
  $ch_d9_instance =  SKYNET_URL;
  $username = SKYNET_USERNAME;
  $password = SKYNET_PASSWORD;
  $client_id = SKYNET_CLIENT_ID;
  $client_secret = SKYNET_CLIENT_SECRET;

  $ch = curl_init();
  curl_setopt($ch, CURLOPT_URL, $ch_d9_instance . '/oauth/token');
  curl_setopt($ch, CURLOPT_POST, 1);
  $post = [
    'username' => $username,
    'password' => $password,
    'client_id' => $client_id,
    'client_secret' => $client_secret,
    'grant_type' => 'password',
    'scope' => 'app',
  ];
  curl_setopt($ch, CURLOPT_POSTFIELDS, $post);
  curl_setopt($ch, CURLOPT_HEADER, false);
  curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
  $response = curl_exec($ch);
  curl_close($ch);
  var_dump($response);
  print_r(json_decode($response, TRUE));
  if (!$res = json_decode($response, TRUE)) {
    return FALSE;
  }
  return $res;
}

function getDmsInstance($token = [], $d9_instance_id) {
  $ch_d9_instance = SKYNET_URL;
  $ch = curl_init();
  curl_setopt($ch, CURLOPT_URL, $ch_d9_instance . '/jsonapi/dms_instance/dms_instance/' . $d9_instance_id);
  $headers = [
    'Accept: application/json',
    'Authorization: Bearer ' . $token['access_token'],
  ];
  curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
  curl_setopt($ch, CURLOPT_HTTPHEADER, $headers);
  $response = curl_exec($ch);
  curl_close($ch);
  if (!$res = json_decode($response, TRUE)) {
    return FALSE;
  }
  return $res['data']['attributes'];
}

/**
 * Implements hook_post_provision_verify().
 */
function drush_provision_canadahelps_post_provision_verify($url = NULL) {
  if (d()->type === 'site') {
    // Only run on CiviCRM sites.
    // For example, the "hostmaster" site does not run CiviCRM.
    if (!provision_symbiotic_civicrm_init()) {
      return;
    }

    // Ignore CiviCRM 4.6 and previous.
    if (! method_exists('Civi', 'settings')) {
      return;
    }

    // Make sure that CiviCRM path/url settings are correct.
    $root = d()->root;
    $host = d()->uri;
    // The "verify" task can run regularly. Check to make sure we only run once,
    // after the initial clone.
    if (Civi::settings()->get('provision_canadahelps_initial_done')) {
      return;
    }
    $d9_id = d()->hosting_canadahelps_d9_id;
    if ($d9_id) {
      $token = login_to_skynet();
      $dms_instance = getDmsInstance($token, $d9_id);
      $smtp_configuration = [
        "outBound_option" => "0",
        "smtpAuth" => "1",
      ];
      if ($dms_instance['smtp_service'] === 'sendgrid') {
        $smtp_configuration['smtpServer'] = "ssl://smtp.sendgrid.net";
        $smtp_configuration['smtpPort'] = "465";
        $smtp_configuration['smtpAuth'] = "1";
        $smtp_configuration['smtpUsername'] = "apikey";
        $smtp_configuration['smtpPassword'] = $dms_instance['smtp_apikey'];
      }
      Civi::settings()->set('mailing_backend', $smtp_configuration);
      $bounceProcessingMailing = civicrm_api3('MailSettings', 'get', ['is_default' => 1]);
      $params = [
        'domain_id' => 1,
        'name' => $dms_instance['email_address'],
        'domain' => 'canadahelps.ca',
        'protocol' => "1",
        'server' => 'imap.gmail.com',
        'username' => $dms_instance['email_address'],
        'password' => $dms_instance['email_password'],
        'is_ssl' => "1",
      ];
      $bounceProcessingMailingAccount = civicrm_api3('MailSettings', 'get', ['is_default' => 1, 'domain_id' => 1]);
      $inboundMailingAccount = civicrm_api3('MailSettings', 'get', ['is_default' => 0, 'domain_id' => 1, 'sequential' => 1]);
      $bounceParams = ['is_default' => 1];
      $inboundParams = ['is_default' => 0];
      if (!empty($bounceProcessingMailingAccount['values'])) {
        $bounceParams['id'] = $bounceProcessingMailingAccount['id'];
      }
      elseif (!empty($inboundMailingAccount['values'])) {
        $inboundParams['id'] = $inboundMailingAccount['values'][0]['id'];
      }
//      civicrm_api3('MailSettings', 'create', array_merge($params, $bounceParams));
      civicrm_api3('MailSettings', 'create', array_merge($params, $inboundParams));
      $check = civicrm_api3('OptionValue', 'get', ['option_group_id' => 'from_email_address', 'value' => '"' . $dms_instance['email_name'] . '" <' . $dms_instance['email_address'] . '>']);
      if (empty($check['values'])) { 
        civicrm_api3('OptionValue', 'create', [
          'option_group_id' => 'from_email_address',
          'value' => '"' . $dms_instance['email_name'] . '" <' . $dms_instance['email_address'] . '>',
          'label' => '"' . $dms_instance['email_name'] . '" <' . $dms_instance['email_address'] . '>',
          'name' => '"' . $dms_instance['email_name'] . '" <' . $dms_instance['email_address'] . '>',
          'is_default' => 1,
        ]);
      }
      civicrm_api3('Extension', 'install', ['key' => 'oauth-client']);
      Civi::settings()->set('provision_canadahelps_initial_done', 1);
    }
  }
}
