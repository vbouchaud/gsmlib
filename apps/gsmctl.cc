// *************************************************************************
// * GSM TA/ME library
// *
// * File:    gsmctl.cc
// *
// * Purpose: GSM mobile phone control program
// *
// * Author:  Peter Hofmann (software@pxh.de)
// *
// * Created: 11.7.1999
// *************************************************************************

#ifdef HAVE_CONFIG_H
#include <gsm_config.h>
#endif
#include <gsmlib/gsm_nls.h>
#include <string>
#if defined(HAVE_GETOPT_LONG) || defined(WIN32)
#include <getopt.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <gsmlib/gsm_me_ta.h>
#include <gsmlib/gsm_util.h>
#include <gsmlib/gsm_sysdep.h>
#ifdef WIN32
#include <gsmlib/gsm_win32_serial.h>
#else
#include <gsmlib/gsm_unix_serial.h>
#include <unistd.h>
#endif
#include <iostream>

// my ME

static gsmlib::MeTa *m;

// information parameters

enum InfoParameter {AllInfo, // print all info
                    MeInfo,     // MeInfo must be first!
                    FunctionalityInfo,
                    OperatorInfo,
                    CurrentOperatorInfo,
                    FacilityLockStateInfo,
                    FacilityLockCapabilityInfo,
                    PasswordInfo,
                    PINInfo,
                    CLIPInfo,
                    CallForwardingInfo,
                    BatteryInfo,
                    BitErrorInfo,
                    SCAInfo,
                    CharSetInfo,
                    SignalInfo}; // SignalInfo must be last!

// operation parameters

// FIXME operations not implemented yet

// options

#ifdef HAVE_GETOPT_LONG
static struct option longOpts[] =
{
  {"xonxoff", no_argument, (int*)NULL, 'X'},
  {"operation", required_argument, (int*)NULL, 'o'},
  {"device", required_argument, (int*)NULL, 'd'},
  {"baudrate", required_argument, (int*)NULL, 'b'},
  {"init", required_argument, (int*)NULL, 'I'},
  {"help", no_argument, (int*)NULL, 'h'},
  {"version", no_argument, (int*)NULL, 'v'},
  {(char*)NULL, 0, (int*)NULL, 0}
};
#else
#define getopt_long(argc, argv, options, longopts, indexptr) \
  getopt(argc, argv, options)
#endif

// helper function, prints forwarding info

void printForwardReason(std::string s, gsmlib::ForwardInfo &info)
{
  std::cout << s << "  "
       << (info._active ? _("active ") : _("inactive "))
       << _("number: ") << info._number
       << _("  subaddr: ") << info._subAddr
       << _("  time: ") << info._time << std::endl;
}

// print information

static void printInfo(InfoParameter ip)
{
  switch (ip)
  {
  case MeInfo:
  {
    gsmlib::MEInfo mei = m->getMEInfo();
    std::cout << _("<ME0>  Manufacturer: ") << mei._manufacturer << std::endl
         << _("<ME1>  Model: ") << mei._model << std::endl
         << _("<ME2>  Revision: ") << mei._revision << std::endl
         << _("<ME3>  Serial Number: ") << mei._serialNumber << std::endl;
    break;
  }
  case FunctionalityInfo:
  {
    try {
      int fun;
      fun = m->getFunctionalityLevel();
      std::cout << _("<FUN>  Functionality Level: ") << fun << std::endl;
    } catch (gsmlib::GsmException &x) { 
      std::cout << _("<FUN>  Functionality Level: ") << _("unsupported") << std::endl;
    }
    break;
  }
  case OperatorInfo:
  {
    int count = 0;
    std::vector<gsmlib::OPInfo> opis = m->getAvailableOPInfo();
    for (std::vector<gsmlib::OPInfo>::iterator i = opis.begin(); i != opis.end(); ++i)
    {
      std::cout << "<OP" << count << _(">  Status: ");
      switch (i->_status)
      {
      case gsmlib::UnknownOPStatus: std::cout << _("unknown"); break;
      case gsmlib::CurrentOPStatus: std::cout << _("current"); break;
      case gsmlib::AvailableOPStatus: std::cout << _("available"); break;
      case gsmlib::ForbiddenOPStatus: std::cout << _("forbidden"); break;
      }
      std::cout << _("  Long name: '") << i->_longName << "' "
           << _("  Short name: '") << i->_shortName << "' "
           << _("  Numeric name: ") << i->_numericName << std::endl;
      ++count;
    }
    break;
  }
  case CurrentOperatorInfo:
  {
    gsmlib::OPInfo opi = m->getCurrentOPInfo();
    std::cout << "<CURROP0>"
         << _("  Long name: '") << opi._longName << "' "
         << _("  Short name: '") << opi._shortName << "' "
         << _("  Numeric name: ") << opi._numericName
         << _("  Mode: ");
    switch (opi._mode)
    {
    case gsmlib::AutomaticOPMode: std::cout << _("automatic"); break;
    case gsmlib::ManualOPMode: std::cout << _("manual"); break;
    case gsmlib::DeregisterOPMode: std::cout << _("deregister"); break;
    case gsmlib::ManualAutomaticOPMode: std::cout << _("manual/automatic"); break;
    }
    std::cout << std::endl;
    break;
  }
  case FacilityLockStateInfo:
  {
    int count = 0;
    std::vector<std::string> fclc = m->getFacilityLockCapabilities();
    for (std::vector<std::string>::iterator i = fclc.begin(); i != fclc.end(); ++i)
      if (*i != "AB" && *i != "AG" && *i != "AC")
      {
        std::cout << "<FLSTAT" << count <<  ">  '" << *i << "'";
        try
        {
          if (m->getFacilityLockStatus(*i, gsmlib::VoiceFacility))
            std::cout << _("  Voice");
        }
        catch (gsmlib::GsmException &e)
	  {
          std::cout << _("  unknown");
        }
        try
        {
	  if (m->getFacilityLockStatus(*i, gsmlib::DataFacility))
	    std::cout << _("  Data");
        }
        catch (gsmlib::GsmException &e)
	  {
          std::cout << _("  unknown");
        }
        try
	  {
	    if (m->getFacilityLockStatus(*i, gsmlib::FaxFacility))
	      std::cout << _("  Fax");
	  }
        catch (gsmlib::GsmException &e)
	  {
	    std::cout << _("  unknown");
	  }
        std::cout << std::endl;
        ++count;
      }
    break;
  }
  case FacilityLockCapabilityInfo:
  {
    std::cout << "<FLCAP0>  ";
    std::vector<std::string> fclc = m->getFacilityLockCapabilities();
    for (std::vector<std::string>::iterator i = fclc.begin(); i != fclc.end(); ++i)
      std::cout << "'" << *i << "' ";
    std::cout << std::endl;
    break;
  }
  case PasswordInfo:
  {
    std::vector<gsmlib::PWInfo> pwi = m->getPasswords();
    int count = 0;
    for (std::vector<gsmlib::PWInfo>::iterator i = pwi.begin(); i != pwi.end(); ++i)
    {
      std::cout << "<PW" << count <<  ">  '"
           << i->_facility << "' " << i->_maxPasswdLen << std::endl;
      ++count;
    }
    break;
  }
  case PINInfo:
  {
    std::cout << "<PIN0> " << m->getPINStatus() << std::endl;
    break;
  }
  case CLIPInfo:
  {
    std::cout << "<CLIP0>  " << (m->getNetworkCLIP() ? _("on") : _("off")) << std::endl;
    break;
  }
  case CallForwardingInfo:
  {
    for (int r = 0; r < 4; ++r)
    {
      std::string text;
      switch (r)
      {
      case 0: text = _("UnconditionalReason"); break;
      case 1: text = _("MobileBusyReason"); break;
      case 2: text = _("NoReplyReason"); break;
      case 3: text = _("NotReachableReason"); break;
      }
      gsmlib::ForwardInfo voice, fax, data;
      m->getCallForwardInfo((gsmlib::ForwardReason)r, voice, fax, data);
      std::cout << "<FORW" << r << ".";
      printForwardReason("0>  " + text + _("  Voice"), voice);
      std::cout << "<FORW" << r << ".";
      printForwardReason("1>  " + text + _("  Data"), data);
      std::cout << "<FORW" << r << ".";
      printForwardReason("2>  " + text + _("  Fax"), fax);
    }
    break;
  }
  case BatteryInfo:
  {
    std::cout << "<BATT0>  ";
    int bcs = m->getBatteryChargeStatus();
    switch (bcs)
    {
    case 0: std::cout << _("0 ME is powered by the battery") << std::endl; break;
    case 1: std::cout << _("1 ME has a battery connected, but is not powered by it")
                 << std::endl; break;
    case 2: std::cout << _("2 ME does not have a battery connected") << std::endl; break;
    case 3:
      std::cout << _("3 Recognized power fault, calls inhibited") << std::endl;
      break;
    }
    std::cout << "<BATT1>  " << m->getBatteryCharge() << std::endl;
    break;
  }
  case BitErrorInfo:
  {
    std::cout << "<BITERR0>  " << m->getBitErrorRate() << std::endl;
    break;
  }
  case SCAInfo:
  {
    std::cout << "<SCA0>  " << m->getServiceCentreAddress() << std::endl;
    break;
  }
  case CharSetInfo:
  {
    std::cout << "<CSET0>  ";
    std::vector<std::string> cs = m->getSupportedCharSets();
    for (std::vector<std::string>::iterator i = cs.begin(); i != cs.end(); ++i)
      std::cout << "'" << *i << "' ";
    std::cout << std::endl;
    std::cout << "<CSET1>  '" << m->getCurrentCharSet() << "'" << std::endl;
    break;
  }
  case SignalInfo:
  {
    std::cout << "<SIG0>  " << m->getSignalStrength() << std::endl;
    break;
  }
  default:
    assert(0);
    break;
  }
}

// convert facility class string of the form "", "all", or any combination
// of "v" (voice), "d" (data), or "f" (fax) to numeric form

gsmlib::FacilityClass strToFacilityClass(std::string facilityClassS)
{
  facilityClassS = gsmlib::lowercase(facilityClassS);
  gsmlib::FacilityClass facilityClass = (gsmlib::FacilityClass)0;
  if (facilityClassS == "all" || facilityClassS == "")
    return (gsmlib::FacilityClass)gsmlib::ALL_FACILITIES;

  // OR in facility class bits
  for (unsigned int i = 0; i < facilityClassS.length(); ++i)
    if (facilityClassS[i] == 'v')
      facilityClass = (gsmlib::FacilityClass)(facilityClass | gsmlib::VoiceFacility);
    else if (facilityClassS[i] == 'd')
      facilityClass = (gsmlib::FacilityClass)(facilityClass | gsmlib::DataFacility);
    else if (facilityClassS[i] == 'f')
      facilityClass = (gsmlib::FacilityClass)(facilityClass | gsmlib::FaxFacility);
    else
      throw gsmlib::GsmException(
        gsmlib::stringPrintf(_("unknown facility class parameter '%c'"),
                     facilityClassS[i]), gsmlib::ParameterError);

  return facilityClass;
}

// check if argc - optind is in range min..max
// throw exception otherwise

void checkParamCount(int optind, int argc, int min, int max)
{
  int paramCount = argc - optind;
  if (paramCount < min)
    throw gsmlib::GsmException(gsmlib::stringPrintf(_("not enough parameters, minimum number "
                                      "of parameters is %d"), min),
                       gsmlib::ParameterError);
  else if (paramCount > max)
    throw gsmlib::GsmException(gsmlib::stringPrintf(_("too many parameters, maximum number "
                                      "of parameters is %d"), max),
                       gsmlib::ParameterError);
}

// *** main program

int main(int argc, char *argv[])
{
  try
  {
    // handle command line options
    std::string device = "/dev/mobilephone";
    std::string operation;
    std::string baudrate;
    std::string initString = gsmlib::DEFAULT_INIT_STRING;
    bool swHandshake = false;

    int opt;
    int dummy;
    while((opt = getopt_long(argc, argv, "I:o:d:b:hvX", longOpts, &dummy))
          != -1)
      switch (opt)
      {
      case 'X':
        swHandshake = true;
        break;
      case 'I':
        initString = optarg;
        break;
      case 'd':
        device = optarg;
        break;
      case 'o':
        operation = optarg;
        break;
      case 'b':
        baudrate = optarg;
        break;
      case 'v':
	std::cerr << argv[0] << gsmlib::stringPrintf(_(": version %s [compiled %s]"),
						     VERSION, __DATE__) << std::endl;
        exit(0);
        break;
      case 'h':
	std::cerr << argv[0] << _(": [-b baudrate][-d device][-h]"
				  "[-I init string][-o operation]\n"
				  "  [-v][-X]{parameters}") << std::endl
		  << std::endl
		  << _("  -b, --baudrate    baudrate to use for device "
		       "(default: 38400)")
		  << std::endl
		  << _("  -d, --device      sets the destination device to "
		       "connect to") << std::endl
		  << _("  -h, --help        prints this message") << std::endl
		  << _("  -I, --init        device AT init sequence") << std::endl
		  << _("  -o, --operation   operation to perform on the mobile \n"
		       "                    phone with the specified parameters")
		  << std::endl
		  << _("  -v, --version     prints version and exits") << std::endl
		  << _("  -X, --xonxoff     switch on software handshake") << std::endl
		  << std::endl
		  << _("  parameters        parameters to use for the operation\n"
		       "                    (if an operation is given) or\n"
		       "                    a specification which kind of\n"
		       "                    information to read from the mobile "
		       "phone")
		  << std::endl << std::endl
		  << _("Refer to gsmctl(1) for details on the available parameters"
		       " and operations.")
		  << std::endl << std::endl;
        exit(0);
        break;
      case '?':
        throw gsmlib::GsmException(_("unknown option"), gsmlib::ParameterError);
        break;
      }

    // open the port and ME/TA
    m = new gsmlib::MeTa(new
#ifdef WIN32
			 gsmlib::Win32SerialPort
#else
			 gsmlib::UnixSerialPort
#endif
			 (device,
			  baudrate == "" ?
			  gsmlib::DEFAULT_BAUD_RATE :
			  gsmlib::baudRateStrToSpeed(baudrate),
			  initString, swHandshake));
    
    if (operation == "")
    {                           // process info parameters
      for (int i = optind; i < argc; ++i)
      {
        std::string param = gsmlib::lowercase(argv[i]);
        if (param == "all")
          for (int ip = MeInfo; ip <= SignalInfo; ++ip)
            printInfo((InfoParameter)ip);
        else if (param == "me")
          printInfo(MeInfo);
        else if (param == "fun")
          printInfo(FunctionalityInfo);
        else if (param == "op")
          printInfo(OperatorInfo);
        else if (param == "currop")
          printInfo(CurrentOperatorInfo);
        else if (param == "flstat")
          printInfo(FacilityLockStateInfo);
        else if (param == "flcap")
          printInfo(FacilityLockCapabilityInfo);
        else if (param == "pw")
          printInfo(PasswordInfo);
        else if (param == "pin")
          printInfo(PINInfo);
        else if (param == "clip")
          printInfo(CLIPInfo);
        else if (param == "forw")
          printInfo(CallForwardingInfo);
        else if (param == "batt")
          printInfo(BatteryInfo);
        else if (param == "biterr")
          printInfo(BitErrorInfo);
        else if (param == "sig")
          printInfo(SignalInfo);
        else if (param == "sca")
          printInfo(SCAInfo);
        else if (param == "cset")
          printInfo(CharSetInfo);
        else
          throw gsmlib::GsmException(
            gsmlib::stringPrintf(_("unknown information parameter '%s'"),
                         param.c_str()),
            gsmlib::ParameterError);
      }
    }
    else
    {                           // process operation
      operation = gsmlib::lowercase(operation);
      if (operation == "dial")
      {
        // dial: number
        checkParamCount(optind, argc, 1, 1);

        m->dial(argv[optind]);
        
        // wait for keypress from stdin
        char c;
        read(1, &c, 1);
      }
      else if (operation == "on")
      {
	  m->setFunctionalityLevel(1);
      }
      else if (operation == "off")
      {
	  m->setFunctionalityLevel(0);
      }
      else if (operation == "pin")
      {
        // pin: PIN
        checkParamCount(optind, argc, 1, 1);

        m->setPIN(argv[optind]);
      }
      else if (operation == "setop")
      {
        // setop: opmode numeric FIXME allow long and numeric too
        checkParamCount(optind, argc, 2, 2);
        std::string opmodeS = gsmlib::lowercase(argv[optind]);
	gsmlib::OPModes opmode;
        if (opmodeS == "automatic")
          opmode = gsmlib::AutomaticOPMode;
        else if (opmodeS == "manual")
          opmode = gsmlib::ManualOPMode;
        else if (opmodeS == "deregister")
          opmode = gsmlib::DeregisterOPMode;
        else if (opmodeS == "manualautomatic")
          opmode = gsmlib::ManualAutomaticOPMode;
        else
          throw gsmlib::GsmException(gsmlib::stringPrintf(_("unknown opmode parameter '%s'"),
                                          opmodeS.c_str()), gsmlib::ParameterError);

        m->setCurrentOPInfo(opmode, "" , "", gsmlib::checkNumber(argv[optind + 1]));
      }
      else if (operation == "lock")
      {
        // lock: facility [facilityclass] [passwd]
        checkParamCount(optind, argc, 1, 3);
        std::string passwd = (argc - optind == 3) ?
          (std::string)argv[optind + 2] : (std::string)"";
        
        m->lockFacility(argv[optind],
                        (argc - optind >= 2) ?
                        strToFacilityClass(argv[optind + 1]) :
                        (gsmlib::FacilityClass)gsmlib::ALL_FACILITIES,
                        passwd);
      }
      else if (operation == "unlock")
      {
        // unlock: facility [facilityclass] [passwd]
        checkParamCount(optind, argc, 1, 3);
        std::string passwd = argc - optind == 3 ? argv[optind + 2] : "";
        
        m->unlockFacility(argv[optind],
                          (argc - optind >= 2) ?
                          strToFacilityClass(argv[optind + 1]) :
                          (gsmlib::FacilityClass)gsmlib::ALL_FACILITIES,
                          passwd);
      }
      else if (operation == "setpw")
      {
        // set password: facility oldpasswd newpasswd
        checkParamCount(optind, argc, 1, 3);
        std::string oldPasswd = argc - optind >= 2 ? argv[optind + 1] : "";
        std::string newPasswd = argc - optind == 3 ? argv[optind + 2] : "";

        m->setPassword(argv[optind], oldPasswd, newPasswd);
      }
      else if (operation == "forw")
      {
        // call forwarding: mode reason number [facilityclass] [forwardtime]
        checkParamCount(optind, argc, 2, 5);

        // get optional parameters facility class and forwardtime
        int forwardTime = argc - optind == 5 ? gsmlib::checkNumber(argv[optind + 4]) :
          gsmlib::NOT_SET;
        gsmlib::FacilityClass facilityClass =
          argc - optind >= 4 ? strToFacilityClass(argv[optind + 3]) :
          (gsmlib::FacilityClass)gsmlib::ALL_FACILITIES;
        
        // get forward reason
        std::string reasonS = gsmlib::lowercase(argv[optind + 1]);
	gsmlib::ForwardReason reason;
        if (reasonS == "unconditional")
          reason = gsmlib::UnconditionalReason;
        else if (reasonS == "mobilebusy")
          reason = gsmlib::MobileBusyReason;
        else if (reasonS == "noreply")
          reason = gsmlib::NoReplyReason;
        else if (reasonS == "notreachable")
          reason = gsmlib::NotReachableReason;
        else if (reasonS == "all")
          reason = gsmlib::AllReasons;
        else if (reasonS == "allconditional")
          reason = gsmlib::AllConditionalReasons;
        else
          throw gsmlib::GsmException(
            gsmlib::stringPrintf(_("unknown forward reason parameter '%s'"),
				 reasonS.c_str()), gsmlib::ParameterError);
        
        // get mode
        std::string modeS = gsmlib::lowercase(argv[optind]);
	gsmlib::ForwardMode mode;
        if (modeS == "disable")
          mode = gsmlib::DisableMode;
        else if (modeS == "enable")
          mode = gsmlib::EnableMode;
        else if (modeS == "register")
          mode = gsmlib::RegistrationMode;
        else if (modeS == "erase")
          mode = gsmlib::ErasureMode;
        else
          throw gsmlib::GsmException(
            gsmlib::stringPrintf(_("unknown forward mode parameter '%s'"),
				 modeS.c_str()), gsmlib::ParameterError);

        m->setCallForwarding(reason, mode,
                             (argc - optind >= 3) ? argv[optind + 2] : "",
                             "", // subaddr
                             facilityClass, forwardTime);
      }
      else if (operation == "setsca")
      {
        // set sca: number
        checkParamCount(optind, argc, 1, 1);
        m->setServiceCentreAddress(argv[optind]);
      }
      else if (operation == "cset")
      {
        // set charset: string
        checkParamCount(optind, argc, 1, 1);
        m->setCharSet(argv[optind]);
      }
      else
         throw gsmlib::GsmException(gsmlib::stringPrintf(_("unknown operation '%s'"),
						 operation.c_str()), gsmlib::ParameterError);
    }
  }
  catch (gsmlib::GsmException &ge)
  {
    std::cerr << argv[0] << _("[ERROR]: ") << ge.what() << std::endl;
    return 1;
  }
  return 0;
}
