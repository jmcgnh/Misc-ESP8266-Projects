<?PHP

header('Content-type: text/plain; charset=utf8', true);

function check_header($name, $value = false) {
    if(!isset($_SERVER[$name])) {
        return false;
    }
    if($value && $_SERVER[$name] != $value) {
        return false;
    }
    return true;
}

function sendFile($path) {
    header($_SERVER["SERVER_PROTOCOL"].' 200 OK', true, 200);
    header('Content-Type: application/octet-stream', true);
    header('Content-Disposition: attachment; filename='.basename($path));
    header('Content-Length: '.filesize($path), true);
    header('x-MD5: '.md5_file($path), true);
    readfile($path);
}

if(!check_header('HTTP_USER_AGENT', 'ESP8266-http-Update')) {
    header($_SERVER["SERVER_PROTOCOL"].' 403 Forbidden', true, 403);
    echo "only for ESP8266 updater!\n";
    exit();
}

if(
    !check_header('HTTP_X_ESP8266_STA_MAC') ||
    !check_header('HTTP_X_ESP8266_AP_MAC') ||
    !check_header('HTTP_X_ESP8266_FREE_SPACE') ||
    !check_header('HTTP_X_ESP8266_SKETCH_SIZE') ||
    !check_header('HTTP_X_ESP8266_CHIP_SIZE') ||
    !check_header('HTTP_X_ESP8266_SDK_VERSION') ||
    !check_header('HTTP_X_ESP8266_VERSION')
) {
    header($_SERVER["SERVER_PROTOCOL"].' 403 Forbidden', true, 403);
    echo "only for ESP8266 updater! (header)\n";
    exit();
}

$macaddr = '';
if(isset($_SERVER['HTTP_X_ESP8266_STA_MAC'])) {
    $macaddr = $_SERVER['HTTP_X_ESP8266_STA_MAC'];
}

$model = 'unknown';

$fp2 = fopen( 'debug_modeldb.txt', 'w');
$fp = fopen( 'modeldb.csv', 'r');
while( ($data = fgetcsv( $fp, 1024, "\t")) != FALSE) {
   fprintf( $fp2, "data row = '%s'\n", serialize( $data));
   if( strcmp( $data[0], $macaddr )) {
      continue;
     } else {
      $model = $data[1];
      break;
     }
   }
fclose( $fp);
fclose( $fp2);

$fp2 = fopen( 'model.txt', 'w');
fprintf( $fp2, "model= %s\n", $model);
fclose( $fp2);

$fp = popen( "/bin/ls -t model/$model | tee filelist.raw", 'r');
$latestfile = trim( fgets( $fp)); /* Newest file should be first ; ignore the rest */
fclose( $fp);

$filepath = "model/$model/$latestfile";

$fp2 = fopen( 'filepath.txt', 'w');
fprintf( $fp2, "filepath = %s\n", $filepath);
fprintf( $fp2, "exists = %s\n", (file_exists( $filepath) ? "TRUE" : "FALSE" ));
fclose( $fp2);

$versionstring = '';

if(isset($_SERVER['HTTP_X_ESP8266_VERSION'])) {
    $versionstring = $_SERVER['HTTP_X_ESP8266_VERSION'] . '.bin';
  }

$fp2 = fopen( 'versionstring.txt', 'w');
fprintf( $fp2, "versionstring = %s\n", $versionstring);
fclose( $fp2);

if( file_exists( $filepath )) {
    if( strcmp( $latestfile, $versionstring) ) {
        sendFile( $filepath);
    } else {
        header($_SERVER["SERVER_PROTOCOL"].' 304 Not Modified', true, 304);
    }
    exit(0); /* these are the only two cases that can be considered 'success' */
}

header($_SERVER["SERVER_PROTOCOL"].' 500 no version for ESP MAC', true, 500);
