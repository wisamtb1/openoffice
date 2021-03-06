:
eval 'exec perl -wS $0 ${1+"$@"}'
    if 0;
#**************************************************************
#  
#  Licensed to the Apache Software Foundation (ASF) under one
#  or more contributor license agreements.  See the NOTICE file
#  distributed with this work for additional information
#  regarding copyright ownership.  The ASF licenses this file
#  to you under the Apache License, Version 2.0 (the
#  "License"); you may not use this file except in compliance
#  with the License.  You may obtain a copy of the License at
#  
#    http://www.apache.org/licenses/LICENSE-2.0
#  
#  Unless required by applicable law or agreed to in writing,
#  software distributed under the License is distributed on an
#  "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
#  KIND, either express or implied.  See the License for the
#  specific language governing permissions and limitations
#  under the License.
#  
#**************************************************************



#
# mapgen  - generate a dependencies file for zip commando 
#
use Cwd;

#### script id #####

( $script_name = $0 ) =~ s/^.*\b(\w+)\.pl$/$1/; 

$id_str = ' $Revision$ ';
$id_str =~ /Revision:\s+(\S+)\s+\$/
  ? ($script_rev = $1) : ($script_rev = "-");

print STDERR "$script_name -- version: $script_rev\n";
print STDERR "Multi Platform Enabled Edition\n";

#########################
#                       #
#   Globale Variablen   #
#                       #
#########################

$zip_file = '';
$R = '';
$r = '';
$exclude = '';
$include = '';
@given_patterns = ();	# patterns(files) list from command line
%files_in_arch = ();
@exc_patterns = (); 	# array of all patterns for files to be excluded
@inc_patterns = (); 	# array of all patterns for files to be included
%exc_files_hash = ();	# hash of files to be excluded (according to @exc_patterns)
%inc_files_hash = ();	# hash of files to be included (according to @inc_patterns)
$prefix = '';

#### main ####

&get_options;
&get_zip_content;
&write_zip_file;

#### end of main procedure ####

#########################
#                       #
#      Procedures       #
#                       #
#########################

#
# procedure writes zipdep file
#
sub write_zip_file {
	my @dependencies = keys %files_in_arch;
	if ($#dependencies != -1) {
		print "\n". &convert_slashes($zip_file) . ' :';
		foreach (@dependencies) {
			next if (-d);
			print " \\\n\t" . $prefix . &convert_slashes($_);
		};
		print "\n\n";
	};
};

#
# convert slashes
#
sub convert_slashes {
	my $path = shift;
	$path =~ s/\//\$\//g;
	$path =~ s/\\/\$\//g;
	if ( $^O eq 'os2' )
	{
		# remove also quotes surrounding name, thus writing buggy paths
		$path =~ s/\"//g;
	}
	return $path;
};

#
# convert slashes to internal perl representation
#
sub perled_slashes {
	my $path = shift;
	$path =~ s/\\/\//g;
	$path =~ s/\/+/\//g;
	if ( $^O eq 'os2' )
	{
		# remove also quotes surrounding name, thus writing buggy paths
		$path =~ s/\"//g;
	}
	return $path;
};

#
# Collect all files to zip in @patterns_array array
#
sub get_zip_content {
	&get_zip_entries(\@given_patterns);
	my $file_name = ''; 
	foreach $file_name (keys %files_in_arch) {
		if (-d $file_name) {
			&get_dir_content($file_name, \%files_in_arch) if ($r || $R);
			undef $files_in_arch{$file_name};
		};
	};
	&remove_uncompliant(\@given_patterns) if ($R);
	&get_patterns_files(\@exc_patterns, \%exc_files_hash) if ($exclude);
	&get_patterns_files(\@inc_patterns, \%inc_files_hash) if ($include);
	foreach my $file_name (keys %exc_files_hash) {
		if (defined $files_in_arch{$file_name}) {
			delete $files_in_arch{$file_name};
			#print STDERR "excluded $file_name\n";
		};
	};
	if ($include) {
		foreach my $file_name (keys %files_in_arch) {
			if (!(defined $inc_files_hash{$file_name})) {
				delete $files_in_arch{$file_name};
			};
		};
	}
};

#
# Procedure removes from %files_in_arch all files which
# are not compliant to patterns in @given_patterns
#
sub remove_uncompliant {
	my $given_patterns = shift;
	my @reg_exps = ();
	my $pattern = '';
	foreach $pattern (@$given_patterns) {
		push(@reg_exps, &make_reg_exp($pattern));
	};
	# write file name as a value for the path(key)	
	foreach my $file (keys %files_in_arch) {
		next if (-d $file);
		#print "$file\n"; 
		if ($file =~ /[\\ | \/](.+)$/) {
			$files_in_arch{$file} = $1;
		} else {
			$files_in_arch{$file} = $file;
		};
	};	
	foreach $pattern (@reg_exps) {
		foreach my $file (keys %files_in_arch) {
			if (!($files_in_arch{$file} =~ /$pattern/)) {
				delete $files_in_arch{$file};
			#} else {
			#	print "Complient: $file\n";
			};
		};
	};
};

#
# Procedure adds/removes to/from %files_in_arch all files, that are 
# compliant to the patterns in array passed 
#
sub get_zip_entries {
	if ($R) {
		opendir DIR, '.';
		my @dir_content = readdir(DIR);
		close DIR;
		foreach my $file_name(@dir_content) {
			$file_name =~ /^\.$/ and next;
			$file_name =~ /^\.\.$/ and next;
			$files_in_arch{$file_name}++;
			#print "included $file_name\n";
		};
	} else {
		my $patterns_array = shift;
		my $pattern = '';
		foreach $pattern (@$patterns_array) {
			if ((-d $pattern) || (-f $pattern)) {
				$files_in_arch{$pattern}++;
				next;
			}
			my $file_name = '';
			foreach $file_name (glob $pattern) {
				#next if (!(-d $file_name) || !(-f $file_name));
				$files_in_arch{$file_name}++;
			};
		};
	} 
};

#
# Procedure converts given parameter to a regular expression
#
sub make_reg_exp {
	my $arg = shift;
	$arg =~ s/\\/\\\\/g;
	$arg =~ s/\//\\\//g;
	$arg =~ s/\./\\\./g;
	$arg =~ s/\+/\\\+/g;
	$arg =~ s/\{/\\\{/g;
	$arg =~ s/\}/\\\}/g;
	$arg =~ s/\*/\.\*/g;
	$arg =~ s/\?/\./g;
	#$arg = '/'.$arg.'/';
	#print "Regular expression:	$arg\n";
	return $arg;
};

#
# Procedure retrieves shell pattern and converts them into regular expressions
#
sub get_patterns {
	my $patterns = shift;
	my $arg = '';
	while ($arg = shift @ARGV) {
		$arg =~ /^-/	and unshift(@ARGV, $arg) and return;
		if (!$zip_file) {
			$zip_file = $arg;
			next;
		};
		$arg = &make_reg_exp($arg);
		push(@$patterns, $arg);
	};
};

#
# Get all options passed
#
sub get_options {
	my ($arg);
	&usage() && exit(0) if ($#ARGV == -1);
	while ($arg = shift @ARGV) {
		$arg = &perled_slashes($arg);
		#print STDERR "$arg\n";
		$arg =~ /^-R$/			and $R = 1	and next;
		$arg =~ /^-r$/			and $r = 1	and next;
		$arg =~ /^-x$/			and $exclude = 1 and &get_patterns(\@exc_patterns) and next;
		$arg =~ /^-i$/			and $include = 1 and &get_patterns(\@inc_patterns) and next;
		$arg =~ /^-prefix$/		and $prefix = shift @ARGV					and next;
		$arg =~ /^-b$/			and shift @ARGV					and next;
		$arg =~ /^-n$/			and shift @ARGV					and next;
		$arg =~ /^-t$/			and shift @ARGV					and next;
		$arg =~ /^-tt$/			and shift @ARGV					and next;
		$arg =~ /^-h$/			and &usage						and exit(0);
		$arg =~ /^--help$/		and &usage						and exit(0);
		$arg =~ /^-?$/			and &usage						and exit(0);
		if ($arg =~ /^-(\w)(\w+)$/) {
			unshift (@ARGV, '-'.$1);
			unshift (@ARGV, '-'.$2);
			next;
		};
# just ignore other switches...		
		$arg =~ /^-(\w+)$/		and	next;
		$arg =~ /^\/\?$/			and &usage						and exit(0);
		$zip_file = $arg		and next if (!$zip_file);
		push(@given_patterns, $arg);
	};
	&print_error('error: Invalid command arguments (do not specify both -r and -R)') if ($r && $R);
	if ($r && ($#given_patterns == -1)) {
		&print_error('no list specified');
	};
};

#
# Procedure fills out passed hash with files from passed dir
# compliant to the pattern from @$patterns
#
sub get_patterns_files {
	my $patterns_array = shift;
	my $files_hash = shift;
	my @zip_files = keys %files_in_arch;
	foreach my $pattern (@$patterns_array) {
		my @fit_pattern = grep /$pattern/, @zip_files;
		foreach my $entry (@fit_pattern) {
			$$files_hash{$entry}++;
			#print "$entry\n";
		};
	};
};

#
# Get dir stuff to pack
#
sub get_dir_content {
	my $dir = shift;
	my $dir_hash_ref = shift;
	my $entry = '';
	if (opendir(DIR, $dir)) {
		my @prj_dir_list = readdir(DIR);
		closedir (DIR);
		foreach $entry (@prj_dir_list) {
			$entry =~ /^\.$/ and next;
			$entry =~ /^\.\.$/ and next;
			
			$entry = $dir . '/' . $entry;
			# if $enry is a dir - read all its files,
			# otherwise store $entry itself
			if (-d $entry) {
				&get_dir_content($entry, $dir_hash_ref);
			} else {
				$$dir_hash_ref{$entry}++;
			};
		};
	};
	return '1';
};

sub print_error {
    my $message = shift;
    print STDERR "\nERROR: $message\n";
	exit (1);
};

sub usage {
	print STDERR "      zipdep  [-aABcdDeEfFghjklLmoqrRSTuvVwXyz]     [-b path]\n";
	print STDERR "      [-n suffixes]  [-t mmddyyyy]  [-tt mmddyyyy]  [  zipfile [\n";
	print STDERR "      file1 file2 ...]] [-xi list]\n";
}

