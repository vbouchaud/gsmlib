#ifdef HAVE_CONFIG_H
#include <gsm_config.h>
#endif
#ifdef WIN32
#include <gsmlib/gsm_win32_serial.h>
#else
#include <gsmlib/gsm_unix_serial.h>
#endif
#include <gsmlib/gsm_me_ta.h>
#include <gsmlib/gsm_phonebook.h>
#include <algorithm>
#include <sstream>
#include <iostream>

int main(int argc, char *argv[])
{
  try
  {
    std::cout << "Opening device " << argv[1] << std::endl;
#ifdef WIN32
    gsmlib::Ref<gsmlib::Port> port = new gsmlib::Win32SerialPort(std::string(argv[1]), 38400);
#else
    gsmlib::Ref<gsmlib::Port> port = new gsmlib::UnixSerialPort(std::string(argv[1]), B38400);
#endif

    std::cout << "Creating MeTa object" << std::endl;
    gsmlib::MeTa m(port);

    std::cout << "Setting message service level to 1" << std::endl;
    m.setMessageService(1);

    std::vector<std::string> storeList = m.getSMSStoreNames();

    for (std::vector<std::string>::iterator stn = storeList.begin();
         stn != storeList.end(); ++stn)
    {
      std::cout << "Getting store \"" << *stn << "\"" << std::endl;
      gsmlib::SMSStoreRef st = m.getSMSStore(*stn);

      gsmlib::SMSMessageRef sms;
      std::cout << "Creating SMS Submit Message and putting it into store" << std::endl;
      gsmlib::SMSSubmitMessage *subsms = new gsmlib::SMSSubmitMessage();
//       Address scAddr("+491710760000");
//       subsms->setServiceCentreAddress(scAddr);
      gsmlib::Address destAddr("0177123456");
      subsms->setDestinationAddress(destAddr);
      subsms->setUserData("This message was sent from the store.");
      gsmlib::TimePeriod tp;
      tp._format = gsmlib::TimePeriod::Relative;
      tp._relativeTime = 100;
      /*subsms->setValidityPeriod(tp);
      subsms->setValidityPeriodFormat(tp._format);
      subsms->setStatusReportRequest(true);*/
      sms = subsms;
      gsmlib::SMSStore::iterator smsIter = st->insert(st->end(), gsmlib::SMSStoreEntry(sms));
      std::cout << "Message entered at index #"
		<< smsIter - st->begin() << std::endl;

      //m.sendSMS(sms);
      gsmlib::SMSMessageRef ackPdu;
      int messageRef = smsIter->send(ackPdu);
      std::cout << "Message reference: " << messageRef << std::endl
		<< "ACK PDU:" << std::endl
		<< (ackPdu.isnull() ? "no ack pdu" : ackPdu->toString())
		<< std::endl;

      /*      cout << "Erasing all unsent messages" << endl;
      for (SMSStore::iterator s = st->begin(); s != st->end(); ++s)
        if (! s->empty() && s->status() == SMSStoreEntry::StoredUnsent)
        st->erase(s);*/

      std::cout << "Printing store \"" << *stn << "\"" << std::endl;
      for (gsmlib::SMSStore::iterator s = st->begin(); s != st->end(); ++s)
        if (!s->empty())
	  std::cout << s->message()->toString();
      break;                    // only do one store
    }
  }
  catch (gsmlib::GsmException &ge)
  {
    std::cerr << "GsmException '" << ge.what() << "'" << std::endl;
    return 1;
  }
  return 0;
}
