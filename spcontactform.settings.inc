<?php

/* ----------------------------------------------------------------------
 * VALIDATION FUNCTIONS
 * ----------------------------------------------------------------------
 *
 * Each key needs a validation function.
 */

function spcontactform_get_data_names() {
  $data_names = array(
    'name' => 'naam',
    'first_name' => 'voornaam',
    'preposition' => 'tussenvoegsel',
    'last_name' => 'achternaam',
    'email' => 'e-mail',
    'telephone' => 'telefoon',
    'street' => 'straat',
    'house_number' => 'huisnummer',
    'house_number_addition' => 'toevoeging',
    'street_and_number' => 'straat en huisnummer',
    'postal_code' => 'postcode',
    'locality' => 'Plaats',
  );
  return $data_names;
}

function spcontactform_valid_name($name) {
  $name = trim($name);
  if (!empty($name)) return $name;
  return;
}

function spcontactform_valid_first_name($first_name) {
  $first_name = trim($first_name);
  if (!empty($first_name)) return $first_name;
  return;
}

function spcontactform_valid_preposition($preposition) {
  $preposition = trim($preposition);
  if (!empty($preposition)) return $preposition;
  return;
}

function spcontactform_valid_last_name($last_name) {
  $last_name = trim($last_name);
  if (!empty($last_name)) return $last_name;
  return;
}
function spcontactform_valid_email($email) {
  $email = trim($email);
  if (valid_email_address($email)) return strtolower($email);
  return;
}

function spcontactform_valid_telephone($telephone) {
  if (!empty($telephone)) return $telephone;
  return;
}

function spcontactform_valid_street($street) {
  $street = trim($street);
  if (!empty($street)) return $street;
  return;
}

function spcontactform_valid_house_number($house_number) {
  $house_number = trim($house_number);
  if (ctype_digit((string)$house_number)) return $house_number;
  return;
}

function spcontactform_valid_house_number_addition($house_number_addition) {
  $house_number_addition = trim($house_number_addition);
  if (!empty($house_number_addition)) return $house_number_addition;
  return;
}

function spcontactform_valid_street_and_number($street_and_number) {
  $street_and_number = trim($street_and_number);
  if (!empty($street_and_number)) return $street_and_number;
  return;
}

function spcontactform_valid_locality($locality) {
  $locality = trim($locality);
  if (!empty($locality)) return $locality;
  return;
}

function spcontactform_valid_postal_code($postal_code) {
  $postal_code = trim($postal_code);
  if (preg_match('@^([1-9][0-9]{3})\s?([a-zA-Z]{2})$@', $postal_code, $matches)) {
    return $matches[1] . ' ' . strtoupper($matches[2]);
  }
  return;
}

