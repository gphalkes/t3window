/* Copyright (C) 2010 G.P. Halkes
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 3, as
   published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#if defined(GENERATE_STRINGS)
#define TEST(_str, _code) send_test_string(_str);
#elif defined(GENERATE_CODE)
#define TEST(_str, _code) if (test++ == report_nr) { _code }
{
	int test = 0;
#endif

/*======================
  = Define TESTs below =
  ======================*/
/* The tests defined here can use the variable column to check the reported width
   of the given test string. Test are executed in the order presented here.
*/
//FIXME: test for more encodings here
//FIXME: extend the GB18030 testing

/*=== Basic character set detection ===*/

/* This string should be 3 characters wide, if UTF-8 is supported. All characters are from
   Unicode version 1.1, so they should be supported if UTF-8 is supported at all. EUC type
   terminals will report length of 6 and single byte encodings will report 8.

   U+00E5 LATIN SMALL LETTER A WITH RING ABOVE,
   U+0E3F THAI CURRENCY SYMBOL BAHT, U+2592 MEDIUM SHADE */
TEST("\xc3\xa5\xe0\xb8\xbf\xe2\x96\x92",
	if (column == 3)
		_t3_term_encoding = _T3_TERM_UTF8;
	else if (column == 6)
		_t3_term_encoding = _T3_TERM_CJK;
)

/* Test for GB18030. For EUC type encodings, this will be length two because the
   bytes with the high bit set will be ignored. For UTF-8, the characters with
   the high bit set will be replaced by the replacement character, thus reporing
   the widht as 4.
   U+00DE LATIN CAPITAL LETTER THORN */
TEST("\x81\x30\x89\x37",
	if (_t3_term_encoding == _T3_TERM_UNKNOWN) {
		if (column == 1)
			_t3_term_encoding = _T3_TERM_GB18030;
		if (column == 2)
			_t3_term_encoding = _T3_TERM_GBK; //FIXME: or GB2312 for some encoding of it
		else if (column == 4)
			_t3_term_encoding = _T3_TERM_SINGLE_BYTE;
	}
)

/*=== Combining character sequences ===*/

/* [4.0] U+002E FULL STOP / U+0350 COMBINING RIGHT ARROWHEAD ABOVE */
TEST("\x2e\xcd\x90", /* UTF-8 version */
	if (_t3_term_encoding == _T3_TERM_UTF8 && column == 1)
		_t3_term_combining = T3_UNICODE_40;
)
TEST("\x2e\x81\x30\xc4\x36", /* GB-18030 version */
	if (_t3_term_encoding == _T3_TERM_GB18030)
		_t3_term_combining = T3_UNICODE_40;
)

/* [4.1] U+002E FULL STOP / U+0358 COMBINING DOT ABOVE RIGHT */
TEST("\x2e\xcd\x98",
	if (_t3_term_encoding == _T3_TERM_UTF8 && column == 1)
		_t3_term_combining = T3_UNICODE_41;
)
/* [5.0] U+002E FULL STOP / U+1DC4 COMBINING MACRON-ACUTE */
TEST("\x2e\xe1\xb7\x84",
	if (_t3_term_encoding == _T3_TERM_UTF8 && column == 1)
		_t3_term_combining = T3_UNICODE_50;
)
/* [5.1] U+002E FULL STOP / U+0487 COMBINING CYRILLIC POKRYTIE */
TEST("\x2e\xd2\x87",
	if (_t3_term_encoding == _T3_TERM_UTF8 && column == 1)
		_t3_term_combining = T3_UNICODE_51;
)
/* [5.2] U+081B SAMARITAN MARK EPENTHETIC YUT */
TEST("\xe0\xa0\x9b",
	if (_t3_term_encoding == _T3_TERM_UTF8 && column == 1)
		_t3_term_combining = T3_UNICODE_52;
)
/* [6.0] U+0859 MANDAIC AFFRICATION MARK */
TEST("\xe0\xa1\x99",
	if (_t3_term_encoding == _T3_TERM_UTF8 && column == 1)
		_t3_term_combining = T3_UNICODE_60;
)

/*=== Double-width character sequences ===*/

/* [1.1] U+5208 CJK UNIFIED IDEOGRAPH-5208, [4.0] U+FE47 PRESENTATION FORM FOR VERTICAL LEFT SQUARE BRACKET */
TEST("\xe5\x88\x88\xef\xb9\x87",
	if (_t3_term_encoding == _T3_TERM_UTF8 && column == 4)
		_t3_term_double_width = T3_UNICODE_40;
)
/* [4.1] U+FE10 PRESENTATION FORM FOR VERTICAL COMMA */
TEST("\xef\xb8\x90",
	if (_t3_term_encoding == _T3_TERM_UTF8 && column == 2)
		_t3_term_double_width = T3_UNICODE_41;
)
/* No new wide characters were introduced in Unicode 5.0. */
/* [5.1] U+31DC CJK STROKE PZ */
TEST("\xe3\x87\x9c",
	if (_t3_term_encoding == _T3_TERM_UTF8 && column == 2)
		_t3_term_double_width = T3_UNICODE_51;
)
/* [5.2] U+3244 CIRCLED IDEOGRAPH QUESTION */
TEST("\xe3\x89\x84",
	if (_t3_term_encoding == _T3_TERM_UTF8 && column == 2)
		_t3_term_double_width = T3_UNICODE_52;
)
/* [6.0] U+31B8 BOPOMOFO LETTER GH */
TEST("\xe3\x86\xb8",
	if (_t3_term_encoding == _T3_TERM_UTF8 && column == 2)
		_t3_term_double_width = T3_UNICODE_60;
)


/*==============================================
  = Do NOT define any TESTs beyond this point. =
  ==============================================*/

#if defined(GENERATE_CODE)
	if (detecting_terminal_capabilities && test - 1 == report_nr) {
		detecting_terminal_capabilities = t3_false;
		finish_detection();
		result = t3_true;
	}
}
#endif
#undef TEST
