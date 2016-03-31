<?php

/* ----------------------------------------------------------------------
 * VALIDATION FUNCTIONS
 * ----------------------------------------------------------------------
 *
 * Each key needs a validation function.
 */

function spwebformsync_get_data_names() {
  $data_names = array(
    'name' => 'gehele naam',
    'first_name' => 'voornaam',
    'middle_name' => 'tussenvoegsel',
    'last_name' => 'achternaam',
    'email' => 'e-mail',
    'telephone' => 'telefoon',
    'street' => 'straat',
    'house_number' => 'huisnummer',
    'house_number_addition' => 'huisnummer toevoeging',
    'street_and_number' => 'straat en huisnummer',
    'postal_code' => 'postcode',
    'locality' => 'plaats',
  );
  return $data_names;
}

function spwebformsync_valid_name($name) {
  $name = trim($name);
  if (!empty($name)) return $name;
}

function spwebformsync_valid_first_name($first_name) {
  $first_name = trim($first_name);
  if (!empty($first_name)) return $first_name;
}

function spwebformsync_valid_middle_name($middle_name) {
  $middle_name = trim($middle_name);
  if (!empty($middle_name)) return $middle_name;
}

function spwebformsync_valid_last_name($last_name) {
  $last_name = trim($last_name);
  if (!empty($last_name)) return $last_name;
}
function spwebformsync_valid_email($email) {
  $email = trim($email);
  if (valid_email_address($email)) return strtolower($email);
}

function spwebformsync_valid_telephone($telephone) {
  if (!empty($telephone)) {
    preg_replace('@[^0-9]@', '', $telephone);
    // +31612345678 => 0649492174
    if (
      strlen($telephone) === 11 &&
      strpos($telephone, '31') === 0
    ) {
      $telephone = substr_replace($telephone, '0', 0, 2);
    }
    // 0031612345678 => 0649492174
    if (
      strlen($telephone) === 13 &&
      strpos($telephone, '0031') === 0
    ) {
      $telephone = substr_replace($telephone, '0', 0, 4);
    }
    return $telephone;
  }
}

function spwebformsync_valid_street($street) {
  $street = trim($street);
  if (!empty($street)) return $street;
}

function spwebformsync_valid_house_number($house_number) {
  $house_number = trim($house_number);
  if (ctype_digit((string)$house_number)) return $house_number;
}

function spwebformsync_valid_house_number_addition($house_number_addition) {
  $house_number_addition = trim($house_number_addition);
  if (!empty($house_number_addition)) return $house_number_addition;
}

function spwebformsync_valid_street_and_number($street_and_number) {
  $street_and_number = trim($street_and_number);
  if (!empty($street_and_number)) return $street_and_number;
}

function spwebformsync_valid_locality($locality) {
  $locality = trim($locality);
  if (!empty($locality)) return $locality;
}

function spwebformsync_valid_postal_code($postal_code) {
  $postal_code = trim($postal_code);
  if (preg_match('@^([1-9][0-9]{3})\s?([a-zA-Z]{2})$@', $postal_code, $matches)) {
    return $matches[1] . ' ' . strtoupper($matches[2]);
  }
}

