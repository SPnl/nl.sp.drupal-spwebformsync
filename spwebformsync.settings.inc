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
    'house_number' => 'huisnummer (inc. toevoeging)',
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
    $telephone = preg_replace('@[^0-9]@', '', $telephone);
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
  // Allow house number and addition, split when parsing data.
  $house_number = trim($house_number);
  if (!empty($house_number)) return $house_number;
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


function spwebformsync_middle_names() {
  return array("‘S","‘s","‘T","‘t","A","a","Aan","aan","Aan ‘t","aan ‘t","Aan de","aan de","Aan den","aan den","Aan der","aan der","Aan het","aan het","Aan t","aan t","Af","af","Al","al","Am","am","Am de","am de","Auf","auf","Auf dem","auf dem","Auf den","auf den","Auf der","auf der","Auf ter","auf ter","Aus","aus","Aus ‘m","aus ‘m","Aus dem","aus dem","Aus den","aus den","Aus der","aus der","Aus m","aus m","Ben","ben","Bij","bij","Bij ‘t","bij ‘t","Bij de","bij de","Bij den","bij den","Bij het","bij het","Bij t","bij t","Bin","bin","Boven d","boven d","Boven d’","boven d’","D","d","D’","d’","Da","da","Dal","dal","Dal’","dal’","Dalla","dalla","Das","das","De","de","De die","de die","De die le","de die le","De l","de l","De l’","de l’","De la","de la","De las","de las","De le","de le","De van der","de van der","Deca","deca","Degli","degli","Dei","dei","Del","del","Della","della","Den","den","Der","der","Des","des","Di","di","Die le","die le","Do","do","Don","don","Dos","dos","Du","du","El","el","Het","het","I","i","Im","im","In","in","In ‘t","in ‘t","In de","in de","In den","in den","In der","in der","In het","in het","In t","in t","L","l","L’","l’","La","la","Las","las","Le","le","Les","les","Lo","lo","Los","los","Of","of","Onder","onder","Onder ‘t","onder ‘t","Onder de","onder de","Onder den","onder den","Onder het","onder het","Onder t","onder t","Op","op","Op ‘t","op ‘t","Op de","op de","Op den","op den","Op der","op der","Op gen","op gen","Op het","op het","Op t","op t","Op ten","op ten","Over","over","Over ‘t","over ‘t","Over de","over de","Over den","over den","Over het","over het","Over t","over t","S","s","S’","s’","T","t","Te","te","Ten","ten","Ter","ter","Tho","tho","Thoe","thoe","Thor","thor","To","to","Toe","toe","Tot","tot","Uijt","uijt","Uijt ‘t","uijt ‘t","Uijt de","uijt de","Uijt den","uijt den","Uijt te de","uijt te de","Uijt ten","uijt ten","Uit","uit","Uit ‘t","uit ‘t","Uit de","uit de","Uit den","uit den","Uit het","uit het","Uit t","uit t","Uit te de","uit te de","Uit ten","uit ten","Unter","unter","Van","van","Van ‘t","van ‘t","Van de","van De","van de","Van de l","van de l","Van de l’","van de l’","Van Den","Van den","van den","Van Der","Van der","van der","Van gen","van gen","Van het","van het","Van la","van la","Van t","van t","Van ter","van ter","Van van de","van van de","Ver","ver","Vom","vom","Von","von","Von ‘t","von ‘t","Von dem","von dem","Von den","von den","Von der","von der","Von t","von t","Voor","voor","Voor ‘t","voor ‘t","Voor de","voor de","Voor den","voor den","Voor in ‘t","voor in ‘t","Voor in t","voor in t","Vor","vor","Vor der","vor der","Zu","zu","Zum","zum","Zur","zur");
}
