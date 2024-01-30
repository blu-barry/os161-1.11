#!/usr/bin/php
<?php
$name = get_current_user();
$dir = '/home/'.$name;

$assignment='ASST0';

/////////////////////
// Change source_tree to the top of the OS/161 source code
//  default: ~/os161/os161-1.11
////////////////////
$source_tree = $dir.'/os161/os161-1.11';

////////////////////
// Change compiled_directory to the location in which you want your binaries compiled
//  default: ~/os161/root
///////////////////
$compiled_directory = $dir.'/os161/root';

if (!file_exists($source_tree)) {
	print "$source_tree does not exist.\n";
	die();
}

print "Attempting to configure ...\n";
chdir($source_tree);
print shell_exec('./configure --ostree='.$compiled_directory);
chdir($source_tree.'/kern/conf');
print shell_exec('./config '.$assignment);
chdir($source_tree.'/kern/compile/'.$assignment);

// Output all errors and warnings to both screen and to ~/os161/scripts/os161_errors.txt
print "Attempting to compile...\n";
print shell_exec('make depend');
print shell_exec('exec 3>&1; make 2>&1 >&3 | tee '.$dir.'/os161/scripts/os161_errors.txt');
print shell_exec('make install');

// Copy over sample config if not already present.
$conf_file = $compiled_directory.'/sys161.conf';
if (!file_exists($conf_file)) {
  shell_exec('cp /usr/local/shared/cs471/config/sys161.conf.sample '.$compiled_directory.'/sys161.conf');
}

print "Attempting to build userspace programs.\n";
chdir($source_tree);
print shell_exec('make');

?>
