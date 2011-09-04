#ifdef HAVE_CONFIG_H
#include <gsm_config.h>
#endif
#include <gsmlib/gsm_sorted_phonebook.h>
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
    // open phonebook file
    gsmlib::SortedPhonebook pb(std::string("spb-copy.pb"), false);
    
    // print all entries
    std::cout << "Entries in pbs-copy.pb:" << std::endl;
    for (gsmlib::SortedPhonebook::iterator i = pb.begin(); i != pb.end(); ++i)
      std::cout << "  Text: " << i->text()
		<< "  Telephone: " << i->telephone() << std::endl;

    // remove all entries with telephone == "0815"
    std::cout << "Removing entries with telephone == 0815" << std::endl;
    pb.setSortOrder(gsmlib::ByTelephone);

    std::string s = "0815";
    pb.erase(s);

    std::cout << "Entries in pbs-copy.pb<2>:" << std::endl;
    for (gsmlib::SortedPhonebook::iterator i = pb.begin(); i != pb.end(); ++i)
      std::cout << "  Text: " << i->text()
		<< "  Telephone: " << i->telephone() << std::endl;
    
    // insert some entries
    std::cout << "Inserting some entries" << std::endl;
    pb.insert(gsmlib::PhonebookEntryBase("08152", "new line with \r continued"));
    pb.insert(gsmlib::PhonebookEntryBase("41598254", "Hans-Dieter Schmidt"));
    pb.insert(gsmlib::PhonebookEntryBase("34058", "Hans-Dieter|Hofmann"));

    pb.setSortOrder(gsmlib::ByText);
    std::cout << "Entries in pbs-copy.pb<3>:" << std::endl;
    for (gsmlib::SortedPhonebook::iterator i = pb.begin(); i != pb.end(); ++i)
      std::cout << "  Text: " << i->text()
		<< "  Telephone: " << i->telephone() << std::endl;

    // test erasing all "Hans-Dieter Schmidt" entries
    std::cout << "Erasing all Hans-Dieter Schmidt entries" << std::endl;
    s = "Hans-Dieter Schmidt";
    std::pair<gsmlib::SortedPhonebook::iterator, gsmlib::SortedPhonebook::iterator> range =
      pb.equal_range(s);
    std::cout << "About to erase:" << std::endl;
    for (gsmlib::SortedPhonebook::iterator i = range.first; i != range.second; ++i)
      std::cout << "  Text: " << i->text()
		<< "  Telephone: " << i->telephone() << std::endl;
    
    pb.erase(range.first, range.second);

    // write back to file
    std::cout << "Writing back to file" << std::endl;
    pb.sync();

    // tests the NoCopy class
    //SortedPhonebook pb2("spb.pb");
    //pb2 = pb;
  }
  catch (gsmlib::GsmException &ge)
  {
    std::cerr << "GsmException '" << ge.what() << "'" << std::endl;
    return 1;
  }
  return 0;
}
