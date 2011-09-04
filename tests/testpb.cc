#include <gsmlib/gsm_unix_serial.h>
#include <gsmlib/gsm_me_ta.h>
#include <gsmlib/gsm_phonebook.h>
#include <algorithm>
#include <iostream>

void printPb(gsmlib::PhonebookEntry &e)
{
  std::cout << "number: " << e.telephone()
	    << " text: " << e.text() << std::endl;
}

int main(int argc, char *argv[])
{
  try
  {
    std::cout << "Opening device " << argv[1] << std::endl;
    gsmlib::Ref<gsmlib::Port> port = new gsmlib::UnixSerialPort(std::string(argv[1]), B38400);

    std::cout << "Creating MeTa object" << std::endl;
    gsmlib::MeTa m(port);

    std::cout << "Getting phonebook entries" << std::endl;
    std::vector<std::string> pbs = m.getPhoneBookStrings();
    for (std::vector<std::string>::iterator i = pbs.begin(); i != pbs.end(); ++i)
    {
      gsmlib::PhonebookRef pb = m.getPhonebook(*i);

      std::cout << "Phonebook \"" << *i << "\" " << std::endl
		<< "  Max number length: " << pb->getMaxTelephoneLen() << std::endl
		<< "  Max text length: " << pb->getMaxTextLen() << std::endl
		<< "  Capacity: " << pb->capacity() << std::endl
		<< "  Size: " << pb->size() << std::endl;

      for (gsmlib::Phonebook::iterator j = pb->begin(); j != pb->end(); ++j)
        if (!j->empty())
	  std::cout << "  Entry #" << j - pb->begin()
		    << "Number: \"" << j->telephone() << "\""
		    << "Text: \"" << j->text() << "\"" << std::endl;
    }
  }
  catch (gsmlib::GsmException &ge)
  {
    std::cerr << "GsmException '" << ge.what() << "'" << std::endl;
    return 1;
  }
  return 0;
}
