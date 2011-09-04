// *************************************************************************
// * GSM TA/ME library
// *
// * File:    testcb.cc
// *
// * Purpose: Test cell broadcast SMS
// *
// * Author:  Peter Hofmann (software@pxh.de)
// *
// * Created: 3.8.2001
// *************************************************************************

#ifdef HAVE_CONFIG_H
#include <gsm_config.h>
#endif
#include <gsmlib/gsm_cb.h>
#include <gsmlib/gsm_nls.h>
#include <gsmlib/gsm_error.h>
#include <iostream>

int main(int argc, char *argv[])
{
  try
  {
    gsmlib::CBMessageRef cbm = new gsmlib::CBMessage("001000320111C3343D0F82C51A8D46A3D168341A8D46A3D168341A8D46A3D168341A8D46A3D168341A8D46A3D168341A8D46A3D168341A8D46A3D168341A8D46A3D168341A8D46A3D168341A8D46A3D168341A8D46A3D100");
    
    std::cout << cbm->toString();
    
    cbm = new gsmlib::CBMessage("001000320111C4EAB3D168341A8D46A3D168341A8D46A3D168341A8D46A3D168341A8D46A3D168341A8D46A3D168341A8D46A3D168341A8D46A3D168341A8D46A3D168341A8D46A3D168341A8D46A3D168341A8D46A3D100");
    
    std::cout << cbm->toString();
  }
  catch (gsmlib::GsmException &ge)
  {
    std::cerr << argv[0] << _("[ERROR]: ") << ge.what() << std::endl;
  }
}
