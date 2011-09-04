// *************************************************************************
// * GSM TA/ME library
// *
// * File:    gsmsendsms.cc
// *
// * Purpose: GSM sms send program
// *
// * Author:  Peter Hofmann (software@pxh.de)
// *
// * Created: 16.7.1999
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
#include <iostream>

// options

#ifdef HAVE_GETOPT_LONG
static struct option longOpts[] =
{
  {"requeststat", no_argument, (int*)NULL, 'r'},
  {"xonxoff", no_argument, (int*)NULL, 'X'},
  {"sca", required_argument, (int*)NULL, 'C'},
  {"device", required_argument, (int*)NULL, 'd'},
  {"init", required_argument, (int*)NULL, 'I'},
  {"concatenate", required_argument, (int*)NULL, 'c'},
  {"baudrate", required_argument, (int*)NULL, 'b'},
  {"test", no_argument, (int*)NULL, 't'},
  {"help", no_argument, (int*)NULL, 'h'},
  {"version", no_argument, (int*)NULL, 'v'},
  {(char*)NULL, 0, (int*)NULL, 0}
};
#else
#define getopt_long(argc, argv, options, longopts, indexptr) \
  getopt(argc, argv, options)
#endif

// convert /r and /n to CR and LF

static std::string unescapeString(char *line)
{
  std::string result;
  bool escaped = false;
  int index = 0;

  while (line[index] != 0 &&
         line[index] != gsmlib::CR && line[index] != gsmlib::LF)
  {
    if (escaped)
    {
      escaped = false;
      if (line[index] == 'r')
        result += gsmlib::CR;
      else if (line[index] == 'n')
        result += gsmlib::LF;
      else if (line[index] == '\\')
        result += '\\';
      else
        result += line[index];
    }
    else
      if (line[index] == '\\')
        escaped = true;
      else
        result += line[index];

    ++index;
  }
  return result;
}

// *** main program

int main(int argc, char *argv[])
{
  try
  {
    // handle command line options
    std::string device = "/dev/mobilephone";
    bool test = false;
    std::string baudrate;
    gsmlib::Ref<gsmlib::GsmAt> at;
    std::string initString = gsmlib::DEFAULT_INIT_STRING;
    bool swHandshake = false;
    bool requestStatusReport = false;
    // service centre address (set on command line)
    std::string serviceCentreAddress;
    gsmlib::MeTa *m = NULL;
    std::string concatenatedMessageIdStr;
    int concatenatedMessageId = -1;

    int opt;
    int dummy;
    while((opt = getopt_long(argc, argv, "c:C:I:d:b:thvXr", longOpts, &dummy))
          != -1)
      switch (opt)
      {
      case 'c':
        concatenatedMessageIdStr = optarg;
        break;
      case 'C':
        serviceCentreAddress = optarg;
        break;
      case 'X':
        swHandshake = true;
        break;
      case 'I':
        initString = optarg;
        break;
      case 'd':
        device = optarg;
        break;
      case 'b':
        baudrate = optarg;
        break;
      case 't':
        test = true;
        break;
      case 'r':
        requestStatusReport = true;
        break;
      case 'v':
	std::cerr << argv[0] << gsmlib::stringPrintf(_(": version %s [compiled %s]"),
						     VERSION, __DATE__) << std::endl;
        exit(0);
        break;
      case 'h':
	std::cerr << argv[0] << _(": [-b baudrate][-c concatenatedID]"
                             "[-C sca][-d device][-h][-I init string]\n"
                             "  [-t][-v][-X] phonenumber [text]") << std::endl
             << std::endl
             << _("  -b, --baudrate    baudrate to use for device "
                  "(default: 38400)")
             << std::endl
             << _("  -c, --concatenate ID for concatenated SMS messages")
             << std::endl
             << _("  -C, --sca         SMS service centre address") << std::endl
             << _("  -d, --device      sets the destination device to connect "
                  "to") << std::endl
             << _("  -h, --help        prints this message") << std::endl
             << _("  -I, --init        device AT init sequence") << std::endl
             << _("  -r, --requeststat request SMS status report") << std::endl
             << _("  -t, --test        convert text to GSM alphabet and "
                  "vice\n"
                  "                    versa, no SMS message is sent") << std::endl
             << _("  -v, --version     prints version and exits")
             << std::endl
             << _("  -X, --xonxoff     switch on software handshake") << std::endl
             << std::endl
             << _("  phonenumber       recipient's phone number") << std::endl
             << _("  text              optional text of the SMS message\n"
                  "                    if omitted: read from stdin")
             << std::endl << std::endl;
        exit(0);
        break;
      case '?':
        throw gsmlib::GsmException(_("unknown option"), gsmlib::ParameterError);
        break;
      }

    if (!test)
    {
      // open the port and ME/TA
      gsmlib::Ref<gsmlib::Port> port = new
#ifdef WIN32
	gsmlib::Win32SerialPort
#else
	gsmlib::UnixSerialPort
#endif
        (device,
         baudrate == "" ? gsmlib::DEFAULT_BAUD_RATE :
         gsmlib::baudRateStrToSpeed(baudrate),
         initString, swHandshake);
      // switch message service level to 1
      // this enables acknowledgement PDUs
      m = new gsmlib::MeTa(port);
      m->setMessageService(1);

      at = new gsmlib::GsmAt(*m);
    }

    // check parameters
    if (optind == argc)
      throw gsmlib::GsmException(_("phone number and text missing"), gsmlib::ParameterError);

    if (optind + 2 < argc)
      throw gsmlib::GsmException(_("more than two parameters given"), gsmlib::ParameterError);
    
    if (concatenatedMessageIdStr != "")
      concatenatedMessageId = gsmlib::checkNumber(concatenatedMessageIdStr);

    // get phone number
    std::string phoneNumber = argv[optind];

    // get text
    std::string text;
    if (optind + 1 == argc)
    {                           // read from stdin
      char s[1000];
      std::cin.get(s, 1000);
      text = unescapeString(s);
      if (text.length() > 160)
        throw gsmlib::GsmException(_("text is larger than 160 characters"),
				   gsmlib::ParameterError);
    }
    else
      text = argv[optind + 1];

    if (test)
      std::cout << gsmlib::gsmToLatin1(gsmlib::latin1ToGsm(text)) << std::endl;
    else
    {
      // send SMS
      gsmlib::Ref<gsmlib::SMSSubmitMessage> submitSMS = new gsmlib::SMSSubmitMessage();
      // set service centre address in new submit PDU if requested by user
      if (serviceCentreAddress != "")
      {
	gsmlib::Address sca(serviceCentreAddress);
        submitSMS->setServiceCentreAddress(sca);
      }
      submitSMS->setStatusReportRequest(requestStatusReport);
      gsmlib::Address destAddr(phoneNumber);
      submitSMS->setDestinationAddress(destAddr);
      if (concatenatedMessageId == -1)
        m->sendSMSs(submitSMS, text, true);
      else
        m->sendSMSs(submitSMS, text, false, concatenatedMessageId);
    }
  }
  catch (gsmlib::GsmException &ge)
  {
    std::cerr << argv[0] << _("[ERROR]: ") << ge.what() << std::endl;
    return 1;
  }
  return 0;
}
