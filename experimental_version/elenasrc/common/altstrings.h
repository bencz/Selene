//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA Common Library
//
//		This header contains String classes declarations
//
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#ifndef altstringsH
#define altstringsH

namespace _ELENA_
{

// --- Constant definition ---
#define STR_PAGE_SIZE               0x0020            // the string page size

// --- _String ---

class _String
{
protected:
   virtual bool _append(const TCHAR* s, size_t length) = 0;

   virtual bool _copy(const TCHAR* s, size_t length) = 0;

   virtual TCHAR* getBody() = 0;

public:
   operator const TCHAR*() const { return asString(); }

   bool isEmpty() const { return emptystr(*this); }

   int Length() const { return getlength(*this); }

   virtual const TCHAR* asString() const = 0;

   virtual void reserve(size_t length) = 0;

   int asInt() const { return _ttoi(*this); }

   void lower() { _tcslwr(getBody()); }

   void upper() { _tcsupr(getBody()); }

   bool copy(const TCHAR* s, size_t length)
   {
      return _copy(s, length);
   }

   bool copy(const TCHAR* s)
   {
      return copy(s, getlength(s));
   }

   bool append(const TCHAR* s, size_t length)
   {
      return _append(s, length);
   }
   bool append(TCHAR ch)
   {
      return append(&ch, 1);
   }
   bool append(const TCHAR* s)
   {
      return append(s, getlength(s));
   }
   bool append(const TCHAR* s1, const TCHAR* s2)
   {
      append(s1);
      append(s2);
      return true;
   }
   bool append(const TCHAR* s1, const TCHAR* s2, const TCHAR* s3)
   {
      append(s1);
      append(s2);
      append(s3);
      return true;
   }
   void appendHex(int n);
   void appendInt(int n);
   void appendLong(long n);
   void appendInt64(__int64 n);
   void appendHex64(__int64 n);
   void appendDouble(double n);

   TCHAR* Clone() const { return _ELENA_::strdup(*this); }

   TCHAR* get(int size)
   {
      reserve(size);

      return getBody();
   }

   virtual void clear() = 0;
};

// --- String ---

class String : public _String
{
protected:
   TCHAR* _string;
   size_t _size;

   void allocate(size_t size)
   {
      freestr(_string);
      createstr(_string, size);
      _size = size;
      _string[0] = '\0';
   }

   void reallocate(size_t size)
   {
      if (_size < size) {
         size_t newSize = ((size - 1) / STR_PAGE_SIZE + 1) * STR_PAGE_SIZE;
         recreatestr(_string, newSize);
         _size = size;
      }
   }

   virtual TCHAR* getBody() { return _string; }

   virtual bool _copy(const TCHAR* s, size_t length);
   virtual bool _append(const TCHAR* s, size_t length);

public:
   virtual const TCHAR* asString() const { return _string; }

   virtual TCHAR* asString() { return _string; }

   TCHAR& operator[](size_t index)
   {
      return *(_string + index);
   }

   virtual void reserve(size_t length)
   {
      reallocate(length);
   }

   void trim(TCHAR ch);

   virtual void clear() { _string[0] = 0; }

#ifdef _UNICODE

   void convert(const char* s);

#endif

   String();
   String(size_t length);
   String(const TCHAR* s);
   String(const TCHAR* s1, const TCHAR* s2);
   String(const TCHAR* s1, const TCHAR* s2, const TCHAR* s3);
   String(const TCHAR* s1, const TCHAR* s2, const TCHAR* s3, const TCHAR* s4);
   String(const TCHAR* s, size_t length);

   virtual ~String() { freestr(_string); }
};

// --- LocalString ---

template <size_t size> class LocalString : public _String
{
protected:
   TCHAR _string[size + 1];

   virtual TCHAR* getBody() { return _string; }

   virtual bool _copy(const TCHAR* s, size_t length)
   {
      if (size >= length) {
         _tcsncpy(_string, s, length);

         _string[length] = 0;
         return true;
      }
      else return false;
   }

   virtual bool _append(const TCHAR* s, size_t length)
   {
      if (size >= getlength(*this) + length) {
         _tcsncat(_string, s, length);

         return true;
      }
      else return false;
   }

public:
   virtual const TCHAR* asString() const { return _string; }

   virtual TCHAR* asString() { return _string; }

   TCHAR& operator[](size_t index)
   {
      return *(_string + index);
   }

   virtual void reserve(size_t length) { }

   virtual void clear() { _string[0] = 0; }

   void trim(TCHAR ch)
   {
      size_t length = getlength(_string);
      while (length > 0 && _string[length - 1] == ch) {
         _string[length - 1] = 0;
         length = getlength(_string);
      }
   }

#ifdef _UNICODE

   void convert(const char* s)
   {
      clear();

      ansiToUnicode(s, _string, strlen(s));
      _string[strlen(s)] = 0;
   }

#endif

   LocalString() { _string[0] = 0; }
   LocalString(size_t length) { _string[0] = 0; }
   LocalString(const TCHAR* s)
   {
      copy(s);
   }
   LocalString(const TCHAR* s1, const TCHAR* s2)
   {
      copy(s1);
      append(s2);
   }
   LocalString(const TCHAR* s, size_t length)
   {
      copy(s, length);
   }
};

// --- ReferenceName ---

template<class String> class ReferenceNameTemplate : public _String
{
   String _string;

   virtual bool _append(const TCHAR* s, size_t length)
   {
      return _string.append(s, length);
   }

   virtual bool _copy(const TCHAR* s, size_t length)
   {
      return _string.copy(s, length);
   }

   virtual TCHAR* getBody() { return asString(); }

public:
   virtual const TCHAR* asString() const { return _string; }

   virtual TCHAR* asString() { return _string.asString(); }

   TCHAR& operator[](size_t index)
   {
      return _string[index];
   }

   virtual void reserve(size_t length)
   {
      _string.reserve(length);
   }

   virtual void clear()
   {
      _string.clear();
   }

   ReferenceNameTemplate()
   {
   }
   ReferenceNameTemplate(const TCHAR* moduleName)
      : _string(moduleName)
   {
   }
   ReferenceNameTemplate(const TCHAR* moduleName, const TCHAR* properName)
      : _string(getlength(moduleName) + getlength(properName) + 2)
   {
      if (!emptystr(moduleName)) {
         _string.append(moduleName);
         _string.append('\'');
      }
      _string.append(properName);
   }
   ReferenceNameTemplate(const TCHAR* moduleName, const TCHAR* properName, const TCHAR* subName)
      : _string(getlength(moduleName) + getlength(properName) + getlength(subName) + 3)
   {
      if (!emptystr(moduleName)) {
         _string.append(moduleName);
         _string.append('\'');
      }
      _string.append(properName);
      if (!emptystr(subName)) {
         _string.append('\'');
         _string.append(subName);
      }
   }

   void pathToName(const TCHAR* path)
   {
      while (!emptystr(path)) {
         if (!emptystr(_string)) {
            _string.append('\'');
         }
         int pos = firstPathSeparatorPos(path);
         if (pos != -1) {
            _string.append(path, pos);
            path += pos + 1;
         }
         else {
            pos = lastchrpos(path, '.');
            if (pos != -1) {
               _string.append(path, pos);
            }
            else _string.append(path);
            break;
         }
      }
   }
};

// --- ELENA Quote String class ---

template<class String> class Quote
{
   String _string;

public:
   operator const TCHAR*() const { return _string; }

   Quote(const TCHAR* string)
      : _string(getlength(string))
   {
      for (size_t i = 1 ; i < getlength(string) ; i++) {
         if (string[i]=='%') {
            i++;
            if (string[i]=='n') {
               _string.append('\n');
            }
            else if (string[i]=='r') {
               _string.append('\r');
            }
            else if (string[i]=='t') {
               _string.append('\t');
            }
            else if (string[i]=='a') {
               _string.append('\a');
            }
            else if (string[i]=='b') {
               _string.append('\b');
            }
            else if (string[i]!='%') {
               size_t j = i;
               while (string[i] >= '0' && string[i]<='9')
                  i++;

               LocalString<12> number(string + j, i - j);
               i--;

               _string.append((TCHAR)number.asInt());
            }
            else _string.append(string[i]);
         }
         else if (string[i]=='"') {
            if (string[i+1]!='"') {
               break;
            }
            else _string.append(string[i++]);
         }
         else _string.append(string[i]);
      }
   }
};

// --- ELENA Namespace Template class ---

template<class String> class NamespaceTemplate
{
   String _string;

public:
   operator const TCHAR*() const { return _string; }

   NamespaceTemplate(const TCHAR* reference)
      : _string(reference, lastchrpos(reference, '\'', 0))
   {
   }

   bool compare(const TCHAR* reference)
   {
      int pos = lastchrpos(reference, '\'', 0);
      if (pos == 0 && getlength(_string)==0) 
         return true;
      else if (getlength(_string)==pos) {
         return compstr(_string, reference, pos);
      }
      else return false;
   }
};

// --- ELENA Namespace Template class ---

template<class String> class IdentifierTemplate
{
   String _string;

public:
   operator const TCHAR*() const { return _string; }

   IdentifierTemplate(const TCHAR* reference)
      : _string(reference + lastchrpos(reference, '\'') + 1)
   {
   }
};

// --- ELENA PrivateMessageTemplate String class ---

template <class String> class PrivateMessageTemplate
{
   String _string;

public:
   operator const TCHAR*() const { return _string; }

   // Private message name could have only up to two level namespace (e.g. win32'api'$asHDC instead of win32'api'factories'$ashdc)
   PrivateMessageTemplate(const TCHAR* moduleName, const TCHAR* privateName)
   {
      int len = getlength(moduleName);
      int pos = chrpos(moduleName, '\'');
      if (pos != -1) {
         int sndpos = chrpos(moduleName + pos + 1, '\'');
         if (sndpos != -1)
            len = sndpos + pos + 1;
      }
      _string.append(moduleName, len);
      _string.append('\'');
      _string.append(privateName);
   }
};

} // _ELENA_

#endif // altstringH
