#include <gsmlib/gsm_unix_serial.h>
#include <gsmlib/gsm_me_ta.h>
#include <gsmlib/gsm_phonebook.h>
#include <algorithm>
#include <iostream>

bool isbla(gsmlib::PhonebookEntry &e)
{
  //  cerr << "****'" << e.text() << "'" << endl;
  return e.text() == "blabla";
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
    if (pbs.begin() == pbs.end())
    {
      std::cerr << "no phonebooks available" << std::endl;
      exit(1);
    }
    
    gsmlib::PhonebookRef pb = m.getPhonebook(*pbs.begin());

    std::cout << "Phonebook \"" << pb->name() << "\" " << std::endl
	      << "  Max number length: " << pb->getMaxTelephoneLen() << std::endl
	      << "  Max text length: " << pb->getMaxTextLen() << std::endl
	      << "  Capacity: " << pb->capacity() << std::endl;

    std::cout << "Inserting entry 'blabla'" << std::endl;
    gsmlib::PhonebookEntry e("123456", "blabla");
    pb->insert(pb->end(), e);

    int j = -1;
    for (int i = 50; i < 60; ++i)
      if (pb()[i].empty())
      {
        pb()[i].set("23456", "blabla");
        j = i;
        break;
      }
    
    pb->erase(pb->begin() + j);

    gsmlib::Phonebook::iterator k;
    do
    {
      k = std::find_if(pb->begin(), pb->end(), isbla);
      if (k != pb->end())
      {
	std::cerr << "Erasing #" << k - pb->begin() << std::endl;
        pb->erase(k, k + 1);
      }
    }
    while (k != pb->end());
  }
  catch (gsmlib::GsmException &ge)
  {
    std::cerr << "GsmException '" << ge.what() << "'" << std::endl;
    return 1;
  }
  return 0;
}
