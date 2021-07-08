<?php

/**
 * Login to D9 Instance using token
 */
function login_to_skynet() {
  $ch_d9_instance =  variable_get('skynet_d9_instance', 'https://ch-d9.jmaconsulting.biz');
  $username = variable_get('skynet_username');
  $password = variable_get('skynet_password');
  $client_id = variable_get('skynet_client_id');
  $client_secret = variable_get('skynet_client_secret');

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
  curl_setopt($ch, CURLOPT_HEADER, ['accept: application/json']);

  $response = curl_exec($ch);
  if (curl_errno($ch)) {
    return FALSE;
  }
  curl_close($ch);
  if (!$res = json_decode($response, TRUE)) {
    return FALSE;
  }
  return $res;
}

function getDmsInstance($token = [], $d9_instance_id) {
  $ch_d9_instance = variable_get('skynet_d9_instance', 'https://ch-d9.jmaconsulting.biz');
  $ch = curl_init();
  curl_setopt($ch, CURLOPT_URL, $ch_d9_instance . '/jsonapi/dms_instance/dms_instance/' . $d9_instance_id);
  $headers = [
    'accept: application/json',
    'Authorization Bearer ' . $token['access_token'],
  ];
  curl_setopt($ch, CURLOPT_HEADER, $headers);
  $response = curl_exec($ch);
  if (curl_errno($ch)) {
    return FALSE;
  }
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
    civicrm_api3('MailSettings', 'create', [
      'domain_id' => 1,
      'name' => $dms_instance['email_address'],
      'is_default' => "1",
      'domain' => 'canadahelps.ca',
      'protocol' => "1",
      'server' => 'imap.gmail.com',
      'username' => $dms_instance['email_address'],
      'password' => $dms_instance['email_password'],
    ]);
    civicrm_api3('OptionValue', 'create', [
      'option_group_id' => 'from_email_address',
      'value' => '"' . $dms_instance['email_name'] . '" <' . $dms_instance['email_name'] . '>',
      'label' => '"' . $dms_instance['email_name'] . '" <' . $dms_instance['email_name'] . '>',
      'name' => '"' . $dms_instance['email_name'] . '" <' . $dms_instance['email_name'] . '>',
      'is_default' => 1,
    ]);
    Civi::settings()->set('provision_canadahelps_initial_done', 1);
  }
}