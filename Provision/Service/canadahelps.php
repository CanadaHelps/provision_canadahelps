<?php

/**
 * The Canada Helps service class.
 */
class Provision_Service_canadahelps extends Provision_Service {
    public $service = 'canadahelps';
  
    /**
     * Add the property to the site context.
     */
    static function subscribe_site($context) {
      $context->setProperty('hosting_canadahelps_d9_id');
    }
  }