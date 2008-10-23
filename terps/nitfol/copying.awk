BEGIN	{
  FS="\"";
  print "/* ==> Do not modify this file!!  It is created automatically";
  print "   by copying.awk.  Modify copying.awk instead.  <== */";
  print "";
  print "#ifdef DEBUGGING";
  print "";
  print "#include \"nitfol.h\"";
  print "";
  print "void show_copying(void)";
  print "{";
  print "  infix_print_fixed_string(";
}
NR == 1,/^[ 	]*NO WARRANTY[ 	]*$/	{
  if ($0 ~ //)
  {
    print "  \"\\n\\n\"";
  }
  else if ($0 !~ /^[ 	]*NO WARRANTY[ 	]*$/) 
  {
    printf "  \"";
    for (i = 1; i <= NF; i++)
    {
      gsub("\011", "        ", $i);
      printf "%s", $i;
    }
    printf "\\n\"\n";
  }
}
/^[	 ]*NO WARRANTY[ 	]*$/	{
  print "  );";
  print "}";
  print "";
  print "void show_warranty(void)";
  print "{";
  print "  infix_print_fixed_string(";
}
/^[ 	]*NO WARRANTY[ 	]*$/, /^[ 	]*END OF TERMS AND CONDITIONS[ 	]*$/{  
  if (! ($0 ~ /^[ 	]*END OF TERMS AND CONDITIONS[ 	]*$/)) 
  {
    printf "  \"";
    for (i = 1; i <= NF; i++)
    {
      gsub("\011", "        ", $i);
      printf "%s", $i;
    }
    printf "\\n\"\n", $NF;
  }
}
END	{
  print "  );";
  print "}";
  print "";
  print "#endif";
}



