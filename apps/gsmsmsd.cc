// *************************************************************************
// * GSM TA/ME library
// *
// * File:    gsmsmsd.cc
// *
// * Purpose: SMS receiver daemon
// *
// * Author:  Peter Hofmann (software@pxh.de)
// *
// * Created: 5.6.1999
// *************************************************************************

#ifdef HAVE_CONFIG_H
#include <gsm_config.h>
#endif
#include <gsmlib/gsm_nls.h>
#include <string>

#ifdef WIN32
#include <io.h>
#include <gsmlib/gsm_util.h>
#include <gsmlib/gsm_win32_serial.h>
#define popen _popen
#define pclose _pclose
#else
#include <gsmlib/gsm_unix_serial.h>
#include <unistd.h>
#include <dirent.h>
#include <syslog.h>
#endif
#if defined(HAVE_GETOPT_LONG) || defined(WIN32)
#include <getopt.h>
#endif

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <fstream>
#include <iostream>
#include <gsmlib/gsm_me_ta.h>
#include <gsmlib/gsm_event.h>
#include <cstring>

#ifdef HAVE_GETOPT_LONG
static struct option longOpts[] =
{
  {"requeststat", no_argument, (int*)NULL, 'r'},
  {"direct", no_argument, (int*)NULL, 'D'},
  {"xonxoff", no_argument, (int*)NULL, 'X'},
  {"init", required_argument, (int*)NULL, 'I'},
  {"store", required_argument, (int*)NULL, 't'},
  {"device", required_argument, (int*)NULL, 'd'},
  {"spool", required_argument, (int*)NULL, 's'},
  {"sent", required_argument, (int*)NULL, 'S'},
  {"failed", required_argument, (int*)NULL, 'F'},
  {"priorities", required_argument, (int*)NULL, 'P'},
#ifndef WIN32
  {"syslog", no_argument, (int*)NULL, 'L'},
#endif
  {"sca", required_argument, (int*)NULL, 'C'},
  {"flush", no_argument, (int*)NULL, 'f'},
  {"concatenate", required_argument, (int*)NULL, 'c'},
  {"action", required_argument, (int*)NULL, 'a'},
  {"baudrate", required_argument, (int*)NULL, 'b'},
  {"help", no_argument, (int*)NULL, 'h'},
  {"version", no_argument, (int*)NULL, 'v'},
  {(char*)NULL, 0, (int*)NULL, 0}
};
#else
#define getopt_long(argc, argv, options, longopts, indexptr) \
  getopt(argc, argv, options)
#endif

// my ME

static gsmlib::MeTa *me = NULL;
std::string receiveStoreName;        // store name for received SMSs

// service centre address (set on command line)

static std::string serviceCentreAddress;

// ID if concatenated messages should be sent

static int concatenatedMessageId = -1;

// signal handler for terminate signal

bool terminateSent = false;

void terminateHandler(int signum)
{
  terminateSent = true;
}

// local class to handle SMS events

struct IncomingMessage
{
  // used if new message is put into store
  int _index;                   // -1 means message want send directly
  std::string _storeName;
  // used if SMS message was sent directly to TA
  gsmlib::SMSMessageRef _newSMSMessage;
  // used if CB message was sent directly to TA
  gsmlib::CBMessageRef _newCBMessage;
  // used in both cases
  gsmlib::GsmEvent::SMSMessageType _messageType;

  IncomingMessage() : _index(-1) {}
};

std::vector<IncomingMessage> newMessages;

class EventHandler : public gsmlib::GsmEvent
{
public:
  // inherited from GsmEvent
  void SMSReception(gsmlib::SMSMessageRef newMessage,
                    gsmlib::GsmEvent::SMSMessageType messageType);
  void CBReception(gsmlib::CBMessageRef newMessage);
  void SMSReceptionIndication(std::string storeName, unsigned int index,
                              gsmlib::GsmEvent::SMSMessageType messageType);

  virtual ~EventHandler() {}
};

void EventHandler::SMSReception(gsmlib::SMSMessageRef newMessage,
                                gsmlib::GsmEvent::SMSMessageType messageType)
{
  IncomingMessage m;
  m._messageType = messageType;
  m._newSMSMessage = newMessage;
  newMessages.push_back(m);
}

void EventHandler::CBReception(gsmlib::CBMessageRef newMessage)
{
  IncomingMessage m;
  m._messageType = gsmlib::GsmEvent::CellBroadcastSMS;
  m._newCBMessage = newMessage;
  newMessages.push_back(m);
}

void EventHandler::SMSReceptionIndication(std::string storeName, unsigned int index,
                                          gsmlib::GsmEvent::SMSMessageType messageType)
{
  IncomingMessage m;
  m._index = index;

  if (receiveStoreName != "" && ( storeName == "MT" || storeName == "mt"))
    m._storeName = receiveStoreName;
  else
    m._storeName = storeName;

  m._messageType = messageType;
  newMessages.push_back(m);
}

// execute action on std::string

void doAction(std::string action, std::string result)
{
  if (action != "")
  {
    FILE *fd = popen(action.c_str(), "w");
    if (fd == NULL)
      throw gsmlib::GsmException(gsmlib::stringPrintf(_("could not execute '%s'"),
						      action.c_str()), gsmlib::OSError);
    fputs(result.c_str(), fd);
    if (ferror(fd))
      throw gsmlib::GsmException(gsmlib::stringPrintf(_("error writing to '%s'"),
						      action.c_str()), gsmlib::OSError);
    pclose(fd);
  }
  else
    // default if no action: output on stdout
    std::cout << result << std::endl;
}

// send all SMS messages in spool dir

bool requestStatusReport = false;

void sendSMS(std::string spoolDirBase, std::string sentDirBase, std::string failedDirBase,
             unsigned int priority, bool enableSyslog, gsmlib::Ref<gsmlib::GsmAt> at)
{
  std::string spoolDir = spoolDirBase;
  std::string sentDir = sentDirBase;
  std::string failedDir = failedDirBase;
  if ( priority >= 1 )
  {
    spoolDir = spoolDirBase + gsmlib::stringPrintf(_("%d"),priority);
    sentDir = sentDirBase + gsmlib::stringPrintf(_("%d"),priority);
    failedDir = failedDirBase + gsmlib::stringPrintf(_("%d"),priority);
  }
  if ( priority > 1 )
    sendSMS(spoolDirBase, sentDirBase, failedDirBase, priority-1, enableSyslog, at);
  if (spoolDirBase != "")
  {
    // look into spoolDir for any outgoing SMS that should be sent
#ifdef WIN32
    struct _finddata_t fileInfo;
	long fileHandle;
	std::string pattern = spoolDir + "\\*";
    fileHandle = _findfirst(pattern.c_str(), &fileInfo);
	bool moreFiles = fileHandle != -1L;
#else
    DIR *dir = opendir(spoolDir.c_str());
    if (dir == (DIR*)NULL)
      throw gsmlib::GsmException(
				 gsmlib::stringPrintf(_("error when calling opendir('%s')"
							"(errno: %d/%s)"), 
						      spoolDir.c_str(), errno, strerror(errno)),
				 gsmlib::OSError);
#endif

#ifdef WIN32
    while (moreFiles)
	{
      if (strcmp(fileInfo.name, ".") != 0 &&
          strcmp(fileInfo.name, "..") != 0)
#else
    struct dirent *entry;
    while ((entry = readdir(dir)) != (struct dirent*)NULL)
      if (strcmp(entry->d_name, ".") != 0 &&
          strcmp(entry->d_name, "..") != 0)
#endif
      {
        if ( priority > 1 )
          sendSMS(spoolDirBase, sentDirBase, failedDirBase, priority-1, enableSyslog, at);
        // read in file
        // the first line is interpreted as the phone number
        // the rest is the message
#ifdef WIN32
        std::string filename = spoolDir + "\\" + fileInfo.name;
#else
        std::string filename = spoolDir + "/" + entry->d_name;
#endif
	std::ifstream ifs(filename.c_str());
        if (! ifs)
	  {
#ifndef WIN32
	    if (enableSyslog)
	      {
		syslog(LOG_WARNING, "Could not open SMS spool file %s",
		       filename.c_str());
		if (failedDirBase != "") {
		  std::string failedfilename = failedDir + "/" + entry->d_name;
		  rename(filename.c_str(),failedfilename.c_str());
		}
		continue;
	      }
	    else
#endif
	      throw gsmlib::GsmException(
					 gsmlib::stringPrintf(_("count not open SMS spool file %s"),
							      filename.c_str()), gsmlib::ParameterError);
	  }
        char phoneBuf[1001];
        ifs.getline(phoneBuf, 1000);
        for (int i=0;i<1000;i++)
          if (phoneBuf[i]=='\t' || phoneBuf[i]==0)
          { // ignore everything after a <TAB> in the phone number
            phoneBuf[i]=0;
            break;
          }
        std::string text;
        while (!ifs.eof())
        {
          char c;
          ifs.get(c);
          text += c;
        }
        ifs.close();
        
        // remove trailing newline/linefeed
        while (text[text.length() - 1] == '\n' ||
               text[text.length() - 1] == '\r')
          text = text.substr(0, text.length() - 1);

        // send the message
        std::string phoneNumber(phoneBuf);
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
        try
        {
          if (concatenatedMessageId == -1)
            me->sendSMSs(submitSMS, text, true);
          else
          {
            // maximum for concatenatedMessageId is 255
            if (concatenatedMessageId > 256)
              concatenatedMessageId = 0;
            me->sendSMSs(submitSMS, text, false, concatenatedMessageId++);
          }
#ifndef WIN32
          if (enableSyslog)
            syslog(LOG_NOTICE, "Sent SMS to %s from file %s", phoneBuf, filename.c_str());
#endif
          if (sentDirBase != "") {
#ifdef WIN32
          std::string sentfilename = sentDir + "\\" + fileInfo.name;
#else
          std::string sentfilename = sentDir + "/" + entry->d_name;
#endif
            rename(filename.c_str(),sentfilename.c_str());
          } else {
            unlink(filename.c_str());
          }
        }
        catch (gsmlib::GsmException &me)
        {
#ifndef WIN32
          if (enableSyslog)
            syslog(LOG_WARNING, "Failed sending SMS to %s from file %s: %s", phoneBuf,
                   filename.c_str(), me.what());
          else
#endif
	    std::cerr << "Failed sending SMS to " << phoneBuf << " from "
		      << filename << ": " << me.what() << std::endl;
          if (failedDirBase != "") {
#ifdef WIN32
            std::string failedfilename = failedDir + "\\" + fileInfo.name;
#else
            std::string failedfilename = failedDir + "/" + entry->d_name;
#endif
            rename(filename.c_str(),failedfilename.c_str());
          }
        }
#ifdef WIN32
      }
      moreFiles = _findnext(fileHandle, &fileInfo) == 0; 
#endif
    }
#ifdef WIN32
    _findclose(fileHandle);
#else
    closedir(dir);
#endif
  }
}

#ifndef WIN32
void syslogExit(int exitcode, int *dummy)
{
  syslog(LOG_NOTICE, "exited (exit %d)",exitcode);
}
#endif

// *** main program

int main(int argc, char *argv[])
{
  bool enableSyslog = false;
  try
  {
    std::string device = "/dev/mobilephone";
    std::string action;
    std::string baudrate;
    bool enableSMS = true;
    bool enableCB = true;
    bool enableStat = true;
    bool flushSMS = false;
    bool onlyReceptionIndication = true;
    std::string spoolDir;
    std::string sentDir = "";
    std::string failedDir = "";
    unsigned int priorities = 0;
    std::string initString = gsmlib::DEFAULT_INIT_STRING;
    bool swHandshake = false;
    std::string concatenatedMessageIdStr;

    int opt;
    int dummy;
    while((opt = getopt_long(argc, argv, "c:C:I:t:fd:a:b:hvs:S:F:P:LXDr",
                             longOpts, &dummy)) != -1)
      switch (opt)
      {
      case 'c':
        concatenatedMessageIdStr = optarg;
        break;
      case 'r':
        requestStatusReport = true;
        break;
      case 'D':
        onlyReceptionIndication = false;
        break;
      case 'X':
        swHandshake = true;
        break;
      case 'I':
        initString = optarg;
        break;
      case 't':
        receiveStoreName = optarg;
        break;
      case 'd':
        device = optarg;
        break;
      case 'C':
        serviceCentreAddress = optarg;
        break;
      case 's':
        spoolDir = optarg;
        break;
      case 'L':
        enableSyslog = true;
        break;
      case 'S':
        sentDir = optarg;
        break;
      case 'F':
        failedDir = optarg;
        break;
      case 'P':
        priorities = abs(atoi(optarg));
        break;
      case 'f':
        flushSMS = true;
        break;
      case 'a':
        action = optarg;
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
	std::cerr << argv[0] << _(": [-a action][-b baudrate][-C sca][-d device]"
                             "[-f][-h][-I init string]\n"
                             "  [-s spool dir][-t][-v]{sms_type}")
             << std::endl << std::endl
             << _("  -a, --action      the action to execute when an SMS "
                  "arrives\n"
                  "                    (SMS is send to stdin of action)")
             << std::endl
             << _("  -b, --baudrate    baudrate to use for device "
                  "(default: 38400)")
             << std::endl
             << _("  -c, --concatenate start ID for concatenated SMS messages")
             << std::endl
             << _("  -C, --sca         SMS service centre address") << std::endl
             << _("  -d, --device      sets the device to connect to") << std::endl
             << _("  -D, --direct      enable direct routing of SMSs") << std::endl
             << _("  -f, --flush       flush SMS from store") << std::endl
             << _("  -F, --failed      directory to move failed SMS to,") << std::endl
             << _("                    if unset, the SMS will be deleted") << std::endl
             << _("  -h, --help        prints this message") << std::endl
             << _("  -I, --init        device AT init sequence") << std::endl
#ifndef WIN32
             << _("  -L, --syslog      log errors and information to syslog")
             << std::endl
#endif
             << _("  -P, --priorities  number of priority levels to use,") << std::endl
             << _("                    (default: none)") << std::endl
             << _("  -r, --requeststat request SMS status report") << std::endl
             << _("  -s, --spool       spool directory for outgoing SMS")
             << std::endl
             << _("  -S, --sent        directory to move sent SMS to,") << std::endl
             << _("                    if unset, the SMS will be deleted") << std::endl
             << _("  -t, --store       name of SMS store to use for flush\n"
                  "                    and/or temporary SMS storage") << std::endl
             << std::endl
             << _("  -v, --version     prints version and exits") << std::endl
             << _("  -X, --xonxoff     switch on software handshake") << std::endl
             << std::endl
             << _("  sms_type may be any combination of") << std::endl << std::endl
             << _("    sms, no_sms     controls reception of normal SMS")
             << std::endl
             << _("    cb, no_cb       controls reception of cell broadcast"
                  " messages") << std::endl
             << _("    stat, no_stat   controls reception of status reports")
             << std::endl << std::endl
             << _("  default is \"sms cb stat\"") << std::endl << std::endl
             << _("If no action is given, the SMS is printed to stdout")
             << std::endl << std::endl
             << _("If -P is given, it activates the priority system and sets the") << std::endl
             << _("number or levels to use. For every level, there must be directories") << std::endl
             << _("named <spool directory>+<priority level>.") << std::endl
             << _("For example \"-P 2 -s queue -S send -F failed\" needs the following") <<std::endl
             << _("directories: queue1/ queue2/ send1/ send2/ failed1/ failed2/") <<std::endl
             << _("Before sending one SMS from queue2, all pending SMS from queue1") <<std::endl
             << _("will be sent.") <<std::endl
             << std::endl << std::endl;
        exit(0);
        break;
      case '?':
        throw gsmlib::GsmException(_("unknown option"), gsmlib::ParameterError);
        break;
      }
  
    // find out which kind of message to route
    for (int i = optind; i < argc; ++i)
    {
      std::string s = gsmlib::lowercase(argv[i]);
      if (s == "sms")
        enableSMS = true;
      else if (s == "no_sms")
        enableSMS = false;
      else if (s == "cb")
        enableCB = true;
      else if (s == "no_cb")
        enableCB = false;
      else if (s == "stat")
        enableStat = true;
      else if (s == "no_stat")
        enableStat = false;
    }

    // check parameters
    if (concatenatedMessageIdStr != "")
      concatenatedMessageId = gsmlib::checkNumber(concatenatedMessageIdStr);
    
    // register signal handler for terminate signal
#ifndef WIN32
    struct sigaction terminateAction;
    terminateAction.sa_handler = terminateHandler;
    sigemptyset(&terminateAction.sa_mask);
    terminateAction.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &terminateAction, NULL) != 0 ||
        sigaction(SIGTERM, &terminateAction, NULL) != 0)
#else
    if(signal(SIGINT, terminateHandler) == SIG_ERR ||
        signal(SIGTERM, terminateHandler) == SIG_ERR)
#endif
      throw gsmlib::GsmException(
				 gsmlib::stringPrintf(_("error when calling sigaction() (errno: %d/%s)"), 
						      errno, strerror(errno)),
				 gsmlib::OSError);

    // open GSM device
    me = new gsmlib::MeTa(new
#ifdef WIN32
			  gsmlib::Win32SerialPort
#else
			  gsmlib::UnixSerialPort
#endif
			  (device,
			   baudrate == "" ? gsmlib::DEFAULT_BAUD_RATE :
			   gsmlib::baudRateStrToSpeed(baudrate), initString,
			   swHandshake));

    // if flush option is given get all SMS from store and dispatch them
    if (flushSMS)
    {
      if (receiveStoreName == "")
        throw gsmlib::GsmException(_("store name must be given for flush option"),
				   gsmlib::ParameterError);
      
      gsmlib::SMSStoreRef store = me->getSMSStore(receiveStoreName);

      for (gsmlib::SMSStore::iterator s = store->begin(); s != store->end(); ++s)
        if (! s->empty())
        {
          std::string result = _("Type of message: ");
          switch (s->message()->messageType())
          {
          case gsmlib::SMSMessage::SMS_DELIVER:
            result += _("SMS message\n");
            break;
          case gsmlib::SMSMessage::SMS_SUBMIT_REPORT:
            result += _("submit report message\n");
            break;
          case gsmlib::SMSMessage::SMS_STATUS_REPORT:
            result += _("status report message\n");
            break;
          }
          result += s->message()->toString();
          doAction(action, result);
          store->erase(s);
        }
    }

    // set default SMS store if -t option was given or
    // read from ME otherwise
    if (receiveStoreName == "")
    {
      std::string dummy1, dummy2;
      me->getSMSStore(dummy1, dummy2, receiveStoreName );
    }
    else
      me->setSMSStore(receiveStoreName, 3);

    // switch message service level to 1
    // this enables SMS routing to TA
    me->setMessageService(1);

    // switch on SMS routing
    me->setSMSRoutingToTA(enableSMS, enableCB, enableStat,
                        onlyReceptionIndication);

    // register event handler to handle routed SMSs, CBMs, and status reports
    me->setEventHandler(new EventHandler());
    
    // wait for new messages
    bool exitScheduled = false;
    while (1)
    {
#ifdef WIN32
      ::timeval timeoutVal;
      timeoutVal.tv_sec = 5;
      timeoutVal.tv_usec = 0;
      me->waitEvent((gsmlib::timeval *)&timeoutVal);
#else
      struct timeval timeoutVal;
      timeoutVal.tv_sec = 5;
      timeoutVal.tv_usec = 0;
      me->waitEvent(&timeoutVal);
#endif
      // if it returns, there was an event or a timeout
      while (newMessages.size() > 0)
      {
        // get first new message and remove it from the vector
	gsmlib::SMSMessageRef newSMSMessage = newMessages.begin()->_newSMSMessage;
	gsmlib::CBMessageRef newCBMessage = newMessages.begin()->_newCBMessage;
	gsmlib::GsmEvent::SMSMessageType messageType =
          newMessages.begin()->_messageType;
        int index = newMessages.begin()->_index;
        std::string storeName = newMessages.begin()->_storeName;
        newMessages.erase(newMessages.begin());

        // process the new message
        std::string result = _("Type of message: ");
        switch (messageType)
        {
        case gsmlib::GsmEvent::NormalSMS:
          result += _("SMS message\n");
          break;
        case gsmlib::GsmEvent::CellBroadcastSMS:
          result += _("cell broadcast message\n");
          break;
        case gsmlib::GsmEvent::StatusReportSMS:
          result += _("status report message\n");
          break;
        }
        if (!newSMSMessage.isnull())
          result += newSMSMessage->toString();
        else if (! newCBMessage.isnull())
          result += newCBMessage->toString();
        else
        {
	  gsmlib::SMSStoreRef store = me->getSMSStore(storeName);
          store->setCaching(false);

          if (messageType == gsmlib::GsmEvent::CellBroadcastSMS)
            result += (*store.getptr())[index].cbMessage()->toString();
          else
            result += (*store.getptr())[index].message()->toString();
            
          store->erase(store->begin() + index);
        }
        
        // call the action
        doAction(action, result);
      }

      // if no new SMS came in and program exit was scheduled, then exit
      if (exitScheduled)
        exit(0);
      
      // handle terminate signal
      if (terminateSent)
      {
        exitScheduled = true;
        // switch off SMS routing
        try
        {
          me->setSMSRoutingToTA(false, false, false);
        }
        catch (gsmlib::GsmException &ge)
        {
          // some phones (e.g. Motorola Timeport 260) don't allow to switch
          // off SMS routing which results in an error. Just ignore this.
        }
        // the AT sequences involved in switching of SMS routing
        // may yield more SMS events, so go round the loop one more time
      }

      // send spooled SMS
      if (!terminateSent)
        sendSMS(spoolDir, sentDir, failedDir, priorities, enableSyslog, me->getAt());
    }
  }
  catch (gsmlib::GsmException &ge)
  {
    std::cerr << argv[0] << _("[ERROR]: ") << ge.what() << std::endl;
    if (ge.getErrorClass() == gsmlib::MeTaCapabilityError)
      std::cerr << argv[0] << _("[ERROR]: ")
           << _("(try setting sms_type, please refer to gsmsmsd manpage)")
           << std::endl;
    // switch off message routing, so that following invocations of gsmsmd
    // are not swamped with message deliveries while they start up
    if (me != NULL)
    {
      try
      {
        me->setSMSRoutingToTA(false, false, false);
      }
      catch (gsmlib::GsmException &ge)
      {
        // some phones (e.g. Motorola Timeport 260) don't allow to switch
        // off SMS routing which results in an error. Just ignore this.
      }
    }
    return 1;
  }
  return 0;
}
