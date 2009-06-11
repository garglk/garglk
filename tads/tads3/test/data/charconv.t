#include <tads.h>
#include <file.h>

translate(inFileName, outFileName)
{
  local inFile, outFile;
  local csMac, csISO;

  /* create the character set objects */
  csMac = new CharacterSet('cp1250');
  csISO = new CharacterSet('cp1252');

  /* open the files */
  inFile = File.openTextFile(inFileName, FileAccessRead, csMac);
  outFile = File.openTextFile(outFileName, FileAccessWrite, csISO);
  if (inFile == nil || outFile == nil)
  {
    "Error: cannot open files.\n";
    return;
  }

  /* read text and write it back out */
  for (;;)
  {
    local txt;

    /* read a line of input; stop if at end of file */
    txt = inFile.readFile();
    if (txt == nil)
      break;

    /* write it out */
    outFile.writeFile(txt);
  }

  /* close the files */
  inFile.closeFile();
  outFile.closeFile();
}

main(args)
{
    try
    {
        translate(args[2], args[3]);
    }
    catch (Exception e)
    {
        "Exception!\n";
        e.displayException();
    }
}
