// *************************************************************************
// * GSM TA/ME library
// *
// * File:    gsmpb.cc
// *
// * Purpose: phonebook management program
// *
// * Author:  Peter Hofmann (software@pxh.de)
// *
// * Created: 24.6.1999
// *************************************************************************

#ifdef HAVE_CONFIG_H
#include <gsm_config.h>
#endif
#include <gsmlib/gsm_nls.h>
#include <string>
#ifdef WIN32
#include <gsmlib/gsm_win32_serial.h>
#else
#include <gsmlib/gsm_unix_serial.h>
#include <unistd.h>
#endif
#if defined(HAVE_GETOPT_LONG) || defined(WIN32)
#include <getopt.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <gsmlib/gsm_me_ta.h>
#include <gsmlib/gsm_util.h>
#include <gsmlib/gsm_sorted_phonebook.h>
#include <iostream>

#ifdef HAVE_GETOPT_LONG
static struct option longOpts[] =
{
  {"xonxoff", no_argument, (int*)NULL, 'X'},
  {"phonebook", required_argument, (int*)NULL, 'p'},
  {"init", required_argument, (int*)NULL, 'I'},
  {"destination", required_argument, (int*)NULL, 'd'},
  {"source", required_argument, (int*)NULL, 's'},
  {"destination-backend", required_argument, (int*)NULL, 'D'},
  {"source-backend", required_argument, (int*)NULL, 'S'},
  {"baudrate", required_argument, (int*)NULL, 'b'},
  {"charset", required_argument, (int*)NULL, 't'},
  {"copy", no_argument, (int*)NULL, 'c'},
  {"synchronize", no_argument, (int*)NULL, 'y'},
  {"help", no_argument, (int*)NULL, 'h'},
  {"version", no_argument, (int*)NULL, 'v'},
  {"verbose", no_argument, (int*)NULL, 'V'},
  {"indexed", no_argument, (int*)NULL, 'i'},
  {(char*)NULL, 0, (int*)NULL, 0}
};
#else
#define getopt_long(argc, argv, options, longopts, indexptr) \
  getopt(argc, argv, options)
#endif

// insert those entries from sourcePhonebook into destPhonebook
// that are not already present in destPhonebook

void insertNotPresent(gsmlib::SortedPhonebookRef sourcePhonebook,
                      gsmlib::SortedPhonebookRef destPhonebook,
                      bool indexed, bool verbose)
{
  for (gsmlib::SortedPhonebookBase::iterator i = sourcePhonebook->begin();
       i != sourcePhonebook->end(); ++i)
  {
    std::pair<gsmlib::SortedPhonebookBase::iterator, gsmlib::SortedPhonebookBase::iterator> range;
    if (indexed)
    {
      int index = i->index();
      range = destPhonebook->equal_range(index);
    }
    else
    {
      std::string text = i->text();
      range = destPhonebook->equal_range(text);
    }

    // do nothing if the entry is already present in the destination
    bool alreadyPresent = false;
    for (gsmlib::SortedPhonebookBase::iterator j = range.first;
         j != range.second; ++j)
    {
      i->setUseIndex(indexed);
      if (i->telephone() == j->telephone())
      {
        alreadyPresent = true;
        break;
      }
    }
    // ... else insert it
    if (!alreadyPresent)
    {
      if (verbose)
      {
	std::cout << gsmlib::stringPrintf(_("inserting '%s' tel# %s"),
                             i->text().c_str(), i->telephone().c_str());
        if (indexed)
	  std::cout << gsmlib::stringPrintf(_(" (index #%d)"), i->index());
	std::cout << std::endl;
      }
      i->setUseIndex(indexed);
      destPhonebook->insert(*i); // insert
    }
  }
}

// update those entries in destPhonebook, that
// - have the same name as one entry in destPhonebook
// - but have a different telephone number
// this is only done if the name in question is unique in the destPhonebook
// the case of several entries having the same in the sourcePhonebook
// is handled - only the first is considered for updating

void updateEntries(gsmlib::SortedPhonebookRef sourcePhonebook,
                   gsmlib::SortedPhonebookRef destPhonebook,
                   bool verbose)
{
  bool firstLoop = true;
  std::string lastText;

  for (gsmlib::SortedPhonebookBase::iterator i = sourcePhonebook->begin();
       i != sourcePhonebook->end(); ++i)
  {
    std::string text = i->text();
    if (!firstLoop && text != lastText)
    {
      std::pair<gsmlib::SortedPhonebookBase::iterator,
		 gsmlib::SortedPhonebookBase::iterator> range =
        destPhonebook->equal_range(text);
      
      gsmlib::SortedPhonebookBase::iterator first = range.first;
      if (first != destPhonebook->end() && range.second == ++first)
      {                         // just one text in the destPhonebook
        if (!(*range.first == *i)) // overwrite if different in destination
        {
          if (verbose)
	    std::cout << gsmlib::stringPrintf(_("updating '%s' tel# %s to new tel# %s"),
					      range.first->text().c_str(),
					      range.first->telephone().c_str(),
					      i->telephone().c_str())
		      << std::endl;
          
          *range.first = *i;
        }
      }
      lastText = text;
    }
    firstLoop = false;
  }
}

// the same but for indexed phonebooks

void updateEntriesIndexed(gsmlib::SortedPhonebookRef sourcePhonebook,
                          gsmlib::SortedPhonebookRef destPhonebook,
                          bool verbose)
{
  for (gsmlib::SortedPhonebookBase::iterator i = sourcePhonebook->begin();
       i != sourcePhonebook->end(); ++i)
  {
    int index = i->index();
    
    gsmlib::SortedPhonebookBase::iterator j = destPhonebook->find(index);
    
    if (j != destPhonebook->end())
    {                           // index present in the destPhonebook
      if (! (*j == *i))         // overwrite if different in destination
      {
        if (verbose)
	  std::cout << gsmlib::stringPrintf(_("updating '%s' tel# %s to new tel# %s"
					      "(index %d)"),
					    j->text().c_str(),
					    j->telephone().c_str(),
					    i->telephone().c_str(), i->index())
		    << std::endl;
        *j = *i;
      }
    }
  }
}

// delete those entries from destPhonebook, that are not present
// in sourcePhonebook

void deleteNotPresent(gsmlib::SortedPhonebookRef sourcePhonebook,
                      gsmlib::SortedPhonebookRef destPhonebook,
                      bool indexed, bool verbose)
{
  for (gsmlib::SortedPhonebookBase::iterator i = destPhonebook->begin();
       i != destPhonebook->end(); ++i)
  {
    std::pair<gsmlib::SortedPhonebookBase::iterator, gsmlib::SortedPhonebookBase::iterator> range;
    if (indexed)
    {
      int index = i->index();
      range = sourcePhonebook->equal_range(index);
    }
    else
    {
      std::string text = i->text();
      range = sourcePhonebook->equal_range(text);
    }
        
    bool found = false;
    for (gsmlib::SortedPhonebookBase::iterator j = range.first;
         j != range.second; ++j)
    {
      i->setUseIndex(indexed);
      if (j->telephone() == i->telephone())
      {
        found = true;
        break;
      }
    }
    if (!found)
    {
      if (verbose)
      {
	std::cout << gsmlib::stringPrintf(_("deleting '%s' tel# %s"),
                             i->text().c_str(), i->telephone().c_str());
        if (indexed)
	  std::cout << gsmlib::stringPrintf(_(" (index #%d)"), i->index());
	std::cout << std::endl;
      }
      destPhonebook->erase(i);
#ifdef BUGGY_MAP_ERASE
	  deleteNotPresent(sourcePhonebook, destPhonebook, indexed, verbose);
	  return;
#endif
    }
  }
}

// *** main program

int main(int argc, char *argv[])
{
  try
  {
    // handle command line options
    std::string destination;
    std::string source;
    std::string destinationBackend;
    std::string sourceBackend;
    std::string baudrate;
    bool doSynchronize = true;
    std::string phonebook;
    gsmlib::SortedPhonebookRef sourcePhonebook, destPhonebook;
    bool verbose = false;
    bool indexed = false;
    std::string initString = gsmlib::DEFAULT_INIT_STRING;
    bool swHandshake = false;
    std::string charSet;
    gsmlib::Ref<gsmlib::MeTa> sourceMeTa, destMeTa;

    int opt;
    int dummy;
    while((opt = getopt_long(argc, argv, "I:p:s:d:b:cyhvViD:S:Xt:", longOpts,
                             &dummy))
          != -1)
      switch (opt)
      {
      case 'X':
        swHandshake = true;
        break;
      case 'I':
        initString = optarg;
        break;
      case 'V':
        verbose = true;
        break;
      case 'p':
        phonebook = optarg;
        break;
      case 'd':
        destination = optarg;
        break;
      case 's':
        source = optarg;
        break;
      case 'D':
        destinationBackend = optarg;
        break;
      case 'S':
        sourceBackend = optarg;
        break;
      case 'b':
        baudrate = optarg;
        break;
      case 't':
        charSet = optarg;
        break;
      case 'c':
        doSynchronize = false;
        break;
      case 'i':
        indexed = true;
        break;
      case 'y':
        doSynchronize = true;
        break;
      case 'v':
	std::cerr << argv[0] << gsmlib::stringPrintf(_(": version %s [compiled %s]"),
						     VERSION, __DATE__) << std::endl;
        exit(0);
        break;
      case 'h':
	std::cerr << argv[0] << _(": [-b baudrate][-c][-d device or file][-h]"
                             "[-I init string]\n"
                             "  [-p phonebook name][-s device or file]"
                             "[-t charset][-v]"
                             "[-V][-y][-X]") << std::endl
             << std::endl
             << _("  -b, --baudrate    baudrate to use for device "
                  "(default: 38400)")
             << std::endl
             << _("  -c, --copy        copy source entries to destination")
             << std::endl
             << _("  -d, --destination sets the destination device to "
                  "connect \n"
                  "                    to, or the file to write") << std::endl
             << _("  -D, --destination-backend sets the destination backend")
             << std::endl
             << _("  -h, --help        prints this message") << std::endl
             << _("  -i, --index       takes index positions into account")
             << std::endl
             << _("  -I, --init        device AT init sequence") << std::endl
             << _("  -p, --phonebook   name of phonebook to use") << std::endl
             << _("  -s, --source      sets the source device to connect to,\n"
                  "                    or the file to read") << std::endl
             << _("  -t, --charset     sets the character set to use for\n"
                  "                    phonebook entries") << std::endl
             << _("  -S, --source-backend sets the source backend")
             << std::endl
             << _("  -v, --version     prints version and exits") << std::endl
             << _("  -V, --verbose     print detailed progress messages")
             << std::endl
             << _("  -X, --xonxoff     switch on software handshake") << std::endl
             << _("  -y, --synchronize synchronize destination with source\n"
                  "                    entries (destination is overwritten)\n"
                  "                    (see gsmpb(1) for details)")
             << std::endl << std::endl;
        exit(0);
        break;
      case '?':
        throw gsmlib::GsmException(_("unknown option"), gsmlib::ParameterError);
        break;
      }

    // check if all parameters all present
    if (destination == "" || source == "")
      throw gsmlib::GsmException(_("both source and destination must be given"),
				 gsmlib::ParameterError);

    // start accessing source mobile phone or file
    if (sourceBackend != "")
      sourcePhonebook =
        gsmlib::CustomPhonebookRegistry::createPhonebook(sourceBackend, source);
    else if (source == "-")
      sourcePhonebook = new gsmlib::SortedPhonebook(true, indexed);
    else if (gsmlib::isFile(source))
      sourcePhonebook = new gsmlib::SortedPhonebook(source, indexed);
    else
    {
      if (phonebook == "")
        throw gsmlib::GsmException(_("phonebook name must be given"), gsmlib::ParameterError);

      sourceMeTa = new gsmlib::MeTa(new
#ifdef WIN32
				    gsmlib::Win32SerialPort
#else
				    gsmlib::UnixSerialPort
#endif
				    (source,
				     baudrate == "" ? gsmlib::DEFAULT_BAUD_RATE :
				     gsmlib::baudRateStrToSpeed(baudrate), initString,
				     swHandshake));
      if (charSet != "")
        sourceMeTa->setCharSet(charSet);
      sourcePhonebook =
        new gsmlib::SortedPhonebook(sourceMeTa->getPhonebook(phonebook));
    }

    // make sure destination.c_str file exists
	if (destination != "")
	{
	  try
	    {
	      std::ofstream f(destination.c_str(), std::ios::out | std::ios::app);
	    }
	  catch (std::exception)
	    {
	    }
	}

    // start accessing destination mobile phone or file
    if (destinationBackend != "")
      destPhonebook =
        gsmlib::CustomPhonebookRegistry::createPhonebook(destinationBackend,
                                                 destination);
    else if (destination == "-")
      destPhonebook = new gsmlib::SortedPhonebook(false, indexed);
    else if (gsmlib::isFile(destination))
      destPhonebook = new gsmlib::SortedPhonebook(destination, indexed);
    else
    {
      if (phonebook == "")
        throw gsmlib::GsmException(_("phonebook name must be given"), gsmlib::ParameterError);

      destMeTa = new gsmlib::MeTa(new 
#ifdef WIN32
				  gsmlib::Win32SerialPort
#else
				  gsmlib::UnixSerialPort
#endif
				  (destination,
				   baudrate == "" ? gsmlib::DEFAULT_BAUD_RATE :
				   gsmlib::baudRateStrToSpeed(baudrate), initString,
				   swHandshake));
      if (charSet != "")
        destMeTa->setCharSet(charSet);
      gsmlib::PhonebookRef destPb = destMeTa->getPhonebook(phonebook);

      // check maximum lengths of source text and phonenumber when writing to
      // mobile phone
      unsigned int maxTextLen = destPb->getMaxTextLen();
      unsigned int maxTelLen = destPb->getMaxTelephoneLen();

      for (gsmlib::SortedPhonebookBase::iterator i = sourcePhonebook->begin();
           i != sourcePhonebook->end(); ++i)
        if (i->text().length() > maxTextLen)
          throw gsmlib::GsmException(
				     gsmlib::stringPrintf(_("text '%s' is too large to fit into destination "
							    "(maximum size %d characters)"),
							  i->text().c_str(), maxTextLen),
				     gsmlib::ParameterError);
        else if (i->telephone().length() > maxTelLen)
          throw gsmlib::GsmException(
				     gsmlib::stringPrintf(_("phone number '%s' is too large to fit into "
							    "destination (maximum size %d characters)"),
							  i->telephone().c_str(), maxTelLen),
				     gsmlib::ParameterError);

      // read phonebook
      destPhonebook = new gsmlib::SortedPhonebook(destPb);      
    }

    // now do the actual work
    if (doSynchronize)
    {                           // synchronizing
      if (indexed)
      {
        sourcePhonebook->setSortOrder(gsmlib::ByIndex);
        destPhonebook->setSortOrder(gsmlib::ByIndex);
        // for an explanation see below
        updateEntriesIndexed(sourcePhonebook, destPhonebook, verbose);
        deleteNotPresent(sourcePhonebook, destPhonebook, true, verbose);
        insertNotPresent(sourcePhonebook, destPhonebook, true, verbose);
      }
      else
      {
        sourcePhonebook->setSortOrder(gsmlib::ByText);
        destPhonebook->setSortOrder(gsmlib::ByText);
        // the following is done to avoid superfluous writes to the TA
        // (that takes time) and keep updated (ie. telephone number changed)
        // entries at the same place
        // 1. update entries in place where just the number changed
        updateEntries(sourcePhonebook, destPhonebook, verbose);
        // 2. delete those that are not present anymore
        deleteNotPresent(sourcePhonebook, destPhonebook, false, verbose);
        // 3. insert the new ones
        insertNotPresent(sourcePhonebook, destPhonebook, false, verbose);
      }
    }
    else
    {                           // copying
      destPhonebook->clear();
      for (gsmlib::SortedPhonebookBase::iterator i = sourcePhonebook->begin();
           i != sourcePhonebook->end(); ++i)
      {
        if (verbose)
        {
	  std::cout << gsmlib::stringPrintf(_("inserting '%s' tel# %s"),
				    i->text().c_str(), i->telephone().c_str());
          if (indexed)
	    std::cout << gsmlib::stringPrintf(_(" (index #%d)"), i->index());
	  std::cout << std::endl;
        }
        destPhonebook->insert(*i);
      }
    }
  }
  catch (gsmlib::GsmException &ge)
  {
    std::cerr << argv[0] << _("[ERROR]: ") << ge.what() << std::endl;
    return 1;
  }
  return 0;
}
