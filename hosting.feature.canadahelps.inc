<?php
/**
 * @file
 *   Expose the Canadhelps feature to hostmaster.
 */

/**
 * Implementation of hook_hosting_feature().
 */
function provision_canadahelps_hosting_feature() {
  $features['canadahelps'] = array(
    'title' => t('CanadaHelps'),
    'description' => t('Enables installation and maintenance of Canadahelps Customisations on Aegir.'),
    'status' => HOSTING_FEATURE_DISABLED,
    'module' => 'provision_canadahelps',
    'group' => 'experimental'
    );
  return $features;
}
