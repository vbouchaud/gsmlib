// *************************************************************************
// * GSM TA/ME library
// *
// * File:    gsm_cb.cc
// *
// * Purpose: Cell Broadcast Message Implementation
// *
// * Author:  Peter Hofmann (software@pxh.de)
// *
// * Created: 4.8.2001
// *************************************************************************

#ifdef HAVE_CONFIG_H
#include <gsm_config.h>
#endif
#include <gsmlib/gsm_sysdep.h>
#include <gsmlib/gsm_cb.h>
#include <gsmlib/gsm_nls.h>
#include <sstream>

using namespace gsmlib;

// local constants

static const std::string dashes =
"---------------------------------------------------------------------------";

// CBDataCodingScheme members

CBDataCodingScheme::CBDataCodingScheme(unsigned char dcs) : _dcs(dcs)
{
  if ((_dcs & 0xf0) <= 0x30)    // bits 7..4 in the range 0000..0011
    {
      if ((_dcs & 0x30) == 0)
	_language = (Language)_dcs;
    }
  else
    _language = (Language)Unknown;
}

std::string CBDataCodingScheme::toString() const
{
  std::string result;
  if (compressed())
    result += _("compressed   ");
  switch (getLanguage())
  {
  case German:
    result += _("German");
    break;
  case English:
    result += _("English");
    break;
  case Italian:
    result += _("Italian");
    break;
  case French:
    result += _("French");
    break;
  case Spanish:
    result += _("Spanish");
    break;
  case Dutch:
    result += _("Dutch");
    break;
  case Swedish:
    result += _("Swedish");
    break;
  case Danish:
    result += _("Danish");
    break;
  case Portuguese:
    result += _("Portuguese");
    break;
  case Finnish:
    result += _("Finnish");
    break;
  case Norwegian:
    result += _("Norwegian");
    break;
  case Greek:
    result += _("Greek");
    break;
  case Turkish:
    result += _("Turkish");
    break;
  case Unknown:
    result += _("Unknown");
    break;
  }
  result += "   ";
  switch (getAlphabet())
  {
  case DCS_DEFAULT_ALPHABET:
    result += _("default alphabet");
    break;
  case DCS_EIGHT_BIT_ALPHABET:
    result += _("8-bit alphabet");
      break;
  case DCS_SIXTEEN_BIT_ALPHABET:
    result += _("16-bit alphabet");
    break;
  case DCS_RESERVED_ALPHABET:
    result += _("reserved alphabet");
    break;
  }
  return result;
}

// CBMessage members

CBMessage::CBMessage(std::string pdu) throw(GsmException)
{
  SMSDecoder d(pdu);
  _messageCode = d.getInteger(6) << 4;
  _geographicalScope = (GeographicalScope)d.get2Bits();
  _updateNumber = d.getInteger(4);
  _messageCode |= d.getInteger(4);
  _messageIdentifier = d.getInteger(8) << 8;
  _messageIdentifier |= d.getInteger(8);
  _dataCodingScheme = CBDataCodingScheme(d.getOctet());
  _totalPageNumber = d.getInteger(4);
  _currentPageNumber = d.getInteger(4);

  // the values 82 and 93 come from ETSI GSM 03.41, section 9.3
  d.markSeptet();
  if (_dataCodingScheme.getAlphabet() == DCS_DEFAULT_ALPHABET)
  {
    _data = d.getString(93);
    _data = gsmToLatin1(_data);
  }
  else
  {
    unsigned char *s = 
      (unsigned char*)alloca(sizeof(unsigned char) * 82);
    d.getOctets(s, 82);
    _data.assign((char*)s, (unsigned int)82);
  }
}

std::string CBMessage::toString() const
{
  std::ostringstream os;
  os << dashes << std::endl
     << _("Message type: CB") << std::endl
     << _("Geographical scope: ");
  switch (_geographicalScope)
  {
  case CellWide:
    os << "Cell wide" << std::endl;
    break;
  case PLMNWide:
    os << "PLMN wide" << std::endl;
    break;
  case LocationAreaWide:
    os << "Location area wide" << std::endl;
    break;
  case CellWide2:
    os << "Cell wide (2)" << std::endl;
    break;
  }
  // remove trailing \r characters for output
  std::string data = _data;
  std::string::iterator i;
  for (i = data.end(); i > data.begin() && *(i - 1) == '\r';
       --i);
  data.erase(i, data.end());

  os << _("Message Code: ") << _messageCode << std::endl
     << _("Update Number: ") << _updateNumber << std::endl
     << _("Message Identifer: ") << _messageIdentifier << std::endl
     << _("Data coding scheme: ") << _dataCodingScheme.toString() << std::endl
     << _("Total page number: ") << _totalPageNumber << std::endl
     << _("Current page number: ") << _currentPageNumber << std::endl
     << _("Data: '") << data << "'" << std::endl
     << dashes << std::endl << std::endl << std::ends;
  return os.str();
}
