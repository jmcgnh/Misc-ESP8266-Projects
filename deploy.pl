#! /bin/env perl
#
# deploy binary to espsite
#
# 2019-11-07 -- jmcg
# 

use strict;
use MIME::Base64;

my $versionline = `grep "String versionstring" *.ino `;
(my $versionstring) = $versionline =~ m{"(.*)"};

printf STDERR "v='%s'\n", $versionstring;
my $binary = `echo *.bin`;
chomp $binary;
printf STDERR "b='%s'\n", $binary;
my $model = $binary;

my $result = `scp $binary  jmcg\@jmcgnh041:/var/tmp/.` or "scp failed";
printf STDERR "r1 = %s\n", $result;
my $commands = <<"EOF";
cp /var/tmp/$binary /var/www/espsite/update/model/$model/$versionstring.bin
chown www-data:www-data /var/www/espsite/update/model/$model/$versionstring.bin
EOF
printf STDERR "-----\nc=%s\n-----\n", $commands;
my $encommands = encode_base64( $commands);
printf STDERR "e=%s", $encommands;

open SCR, ">",  "./script";
print SCR $encommands;

$result = `scp ./script jmcg\@jmcgnh041:/var/tmp/.` or "scp2 failed";

printf STDERR "r2=%s", $result;

$result = `ssh jmcg\@jmcgnh041 'base64 -d --ignore-garbage </var/tmp/script | sudo bash -ex'` or "ssh failed";

printf STDERR "r3=%s", $result;

# can now send a signal to a device for it to check for a new version immediately insteal of waiting out the OTA_update_interval

if( exists( $ENV{DEPLOYTARGET}) ) { 
    my $target = $ENV{DEPLOYTARGET};
    $result = `curl http://$target/`;
    printf STDERR "r4=%s", $result;
   }
