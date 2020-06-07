using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using System.Xml;


// sourced from https://github.com/PTwr/WotTools

public class Packed_Section
{
    public static readonly Int32 Packed_Header = 0x62A14E45;
    public static readonly char[] intToBase64 = new char[] { 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/' };
    public const int MAX_LENGTH = 256;

    public class DataDescriptor
    {
        public readonly int address;
        public readonly int end;
        public readonly int type;

        public DataDescriptor(int end, int type, int address)
        {
            this.end = end;
            this.type = type;
            this.address = address;
        }

        public override string ToString()
        {
            StringBuilder sb = new StringBuilder("[");
            sb.Append("0x");
            sb.Append(Convert.ToString(end, 16));
            sb.Append(", ");
            sb.Append("0x");
            sb.Append(Convert.ToString(type, 16));
            sb.Append("]@0x");
            sb.Append(Convert.ToString(address, 16));
            return sb.ToString();
        }
    }

    public class ElementDescriptor
    {
        public readonly int nameIndex;
        public readonly DataDescriptor dataDescriptor;

        public ElementDescriptor(int nameIndex, DataDescriptor dataDescriptor)
        {
            this.nameIndex = nameIndex;
            this.dataDescriptor = dataDescriptor;
        }

        public override string ToString()
        {
            StringBuilder sb = new StringBuilder("[");
            sb.Append("0x");
            sb.Append(Convert.ToString(nameIndex, 16));
            sb.Append(":");
            sb.Append(dataDescriptor);
            return sb.ToString();
        }
    }

    public void WriteStringWithZero(BinaryWriter writer, string s)
    {
        foreach(var c in s)
        {
            writer.Write(c);
        }
        writer.Write(Convert.ToChar(0x0));
    }

    public string readStringTillZero(BinaryReader reader)
    {
        char[] work = new char[256];

        int i = 0;

        char c = reader.ReadChar();
        while (c != Convert.ToChar(0x0))
        {
            work[System.Math.Max(System.Threading.Interlocked.Increment(ref i), i - 1)] = c;//WHAT?!?!?!
            c = reader.ReadChar();
        }
        string s = "";
        for (var k = 1; k <= i; k++) //WHAT?!
            s = s + work[k];
        return s;
    }

    public void writeDictionary(BinaryWriter writer, List<string> dictionary)
    {
        foreach(var text in dictionary)
        {
            WriteStringWithZero(writer, text);
        }
        writer.Write((byte)0);
    }

    public List<string> readDictionary(BinaryReader reader)
    {
        List<string> dictionary = new List<string>();
        int counter = 0;
        string text = readStringTillZero(reader);

        while (!(text.Length == 0))
        {
            dictionary.Add(text);
            text = readStringTillZero(reader);
            counter += 1;
        }
        return dictionary;
    }

    public void WriteLittleEndianShort(BinaryWriter writer, short s)
    {
        writer.Write(s);
    }
    public int readLittleEndianShort(BinaryReader reader)
    {
        int LittleEndianShort = reader.ReadInt16();
        return LittleEndianShort;
    }

    public int readLittleEndianInt(BinaryReader reader)
    {
        int LittleEndianInt = reader.ReadInt32();
        return LittleEndianInt;
    }
    public long readLittleEndianlong(BinaryReader reader)
    {
        Int64 LittleEndianlong = reader.ReadInt64();
        return LittleEndianlong;
    }

    public DataDescriptor readDataDescriptor(BinaryReader reader)
    {
        int selfEndAndType = readLittleEndianInt(reader);
        return new DataDescriptor(selfEndAndType & 0xFFFFFFF, selfEndAndType >> 28, System.Convert.ToInt32(reader.BaseStream.Position));
    }

    public ElementDescriptor[] readElementDescriptors(BinaryReader reader, int number)
    {
        ElementDescriptor[] elements = new ElementDescriptor[number - 1 + 1];
        for (int i = 0; i <= number - 1; i++)
        {            
            int nameIndex = readLittleEndianShort(reader);
            DataDescriptor dataDescriptor = readDataDescriptor(reader);
            elements[i] = new ElementDescriptor(nameIndex, dataDescriptor);
        }
        return elements;
    }

    public string readString(BinaryReader reader, int lengthInBytes)
    {
        if (lengthInBytes == 0)
        {
            //System.Diagnostics.Debugger.Break();
        }
        string rString = new string(reader.ReadChars(lengthInBytes), 0, lengthInBytes);

        return rString;
    }

    public string readNumber(BinaryReader reader, int lengthInBytes)
    {
        string Number = "";
        switch (lengthInBytes)
        {
            case 1:
                {
                    Number = Convert.ToString(reader.ReadSByte());
                    break;
                    break;
                }

            case 2:
                {
                    Number = Convert.ToString(readLittleEndianShort(reader));
                    break;
                    break;
                }

            case 4:
                {
                    Number = Convert.ToString(readLittleEndianInt(reader));
                    break;
                    break;
                }

            case 8:
                {
                    Number = Convert.ToString(readLittleEndianlong(reader));
                    break;
                    break;
                }

            default:
                {
                    Number = "0";
                    break;
                    break;
                }
        }
        return Number;
    }

    public float readLittleEndianFloat(BinaryReader reader)
    {
        float LittleEndianFloat = reader.ReadSingle();
        return LittleEndianFloat;
    }

    public string readFloats(BinaryReader reader, int lengthInBytes)
    {
        int n = lengthInBytes / 4;

        StringBuilder sb = new StringBuilder();
        for (int i = 0; i <= n - 1; i++)
        {
            if (i != 0)
                sb.Append(" ");
            float rFloat = readLittleEndianFloat(reader);
            sb.Append(rFloat.ToString("0.000000", CultureInfo.InvariantCulture));
        }
        return sb.ToString();
    }


    public bool readBoolean(BinaryReader reader, int lengthInBytes)
    {
        bool @bool = lengthInBytes == 1;
        if (@bool)
        {
            if (reader.ReadSByte() != 1)
                throw new System.ArgumentException("Boolean error");
        }

        return @bool;
    }

    private static string byteArrayToBase64(sbyte[] a)
    {
        int aLen = a.Length;
        int numFullGroups = aLen / 3;
        int numBytesInPartialGroup = aLen - 3 * numFullGroups;
        int resultLen = 4 * ((aLen + 2) / 3);
        StringBuilder result = new StringBuilder(resultLen);

        int inCursor = -1;
        for (int i = 0; i <= numFullGroups - 1; i++)
        {
            int byte0 = a[System.Math.Max(System.Threading.Interlocked.Increment(ref inCursor), inCursor - 1)] & 0xFF;
            inCursor = inCursor;
            int byte1 = a[System.Math.Max(System.Threading.Interlocked.Increment(ref inCursor), inCursor - 1)] & 0xFF;
            inCursor = inCursor;
            int byte2 = a[System.Math.Max(System.Threading.Interlocked.Increment(ref inCursor), inCursor - 2)] & 0xFF;
            result.Append(intToBase64[byte0 >> 2]);
            result.Append(intToBase64[(byte0 << 4) & 0x3F | (byte1 >> 4)]);
            result.Append(intToBase64[(byte1 << 2) & 0x3F | (byte2 >> 6)]);
            result.Append(intToBase64[byte2 & 0x3F]);
        }

        if (numBytesInPartialGroup != 0)
        {
            int byte0 = a[System.Math.Max(System.Threading.Interlocked.Increment(ref inCursor), inCursor - 1)] & 0xFF;
            result.Append(intToBase64[byte0 >> 2]);
            if (numBytesInPartialGroup == 1)
            {
                result.Append(intToBase64[(byte0 << 4) & 0x3F]);
                result.Append("==");
            }
            else
            {
                int byte1 = a[System.Math.Max(System.Threading.Interlocked.Increment(ref inCursor), inCursor - 1)] & 0xFF;
                result.Append(intToBase64[(byte0 << 4) & 0x3F | (byte1 >> 4)]);
                result.Append(intToBase64[(byte1 << 2) & 0x3F]);
                result.Append('=');
            }
        }

        return result.ToString();
    }

    public string readBase64(BinaryReader reader, int lengthInBytes)
    {
        sbyte[] bytes = new sbyte[lengthInBytes - 1 + 1];
        for (int i = 0; i <= lengthInBytes - 1; i++)
            bytes[i] = reader.ReadSByte();
        return byteArrayToBase64(bytes);
    }

    public string readAndToHex(BinaryReader reader, int lengthInBytes)
    {
        sbyte[] bytes = new sbyte[lengthInBytes - 1 + 1];
        for (int i = 0; i <= lengthInBytes - 1; i++)
            bytes[i] = reader.ReadSByte();
        StringBuilder sb = new StringBuilder("[ ");
        foreach (byte b in bytes)
        {
            sb.Append(Convert.ToString((b & 0xFF), 16));
            sb.Append(" ");
        }
        sb.Append("]L:");
        sb.Append(lengthInBytes);

        return sb.ToString();
    }

    public int readData(BinaryReader reader, List<string> dictionary, XmlNode element, XmlDocument xDoc, int offset, DataDescriptor dataDescriptor)
    {
        int lengthInBytes = dataDescriptor.end - offset;
        if (dataDescriptor.type == 0x0)
            // Element                
            readElement(reader, element, xDoc, dictionary);
        else if (dataDescriptor.type == 0x1)
        {
            // String

            element.InnerText = readString(reader, lengthInBytes);
            //Console.WriteLine(element.InnerText);
        }
        else if (dataDescriptor.type == 0x2)
        {
            // Integer number
            element.InnerText = readNumber(reader, lengthInBytes);
            //Console.WriteLine(element.InnerText);
        }
        else if (dataDescriptor.type == 0x3)
        {
            // Floats
            string str = readFloats(reader, lengthInBytes);
            //Console.WriteLine(str);

            string[] strData = str.Split(' ');
            if (strData.Length == 12)
            {
                XmlNode row0 = xDoc.CreateElement("row0");
                XmlNode row1 = xDoc.CreateElement("row1");
                XmlNode row2 = xDoc.CreateElement("row2");
                XmlNode row3 = xDoc.CreateElement("row3");
                row0.InnerText = strData[0] + " " + strData[1] + " " + strData[2];
                row1.InnerText = strData[3] + " " + strData[4] + " " + strData[5];
                row2.InnerText = strData[6] + " " + strData[7] + " " + strData[8];
                row3.InnerText = strData[9] + " " + strData[10] + " " + strData[11];
                element.AppendChild(row0);
                element.AppendChild(row1);
                element.AppendChild(row2);
                element.AppendChild(row3);
            }
            else
                element.InnerText = str;
        }
        else if (dataDescriptor.type == 0x4)
        {
            // Boolean

            if (readBoolean(reader, lengthInBytes))
                element.InnerText = "true";
            else
                element.InnerText = "false";
        }
        else if (dataDescriptor.type == 0x5)
            // Base64
            element.InnerText = readBase64(reader, lengthInBytes);
        else
            throw new System.ArgumentException("Unknown type of \"" + element.Name + ": " + dataDescriptor.ToString() + " " + readAndToHex(reader, lengthInBytes));

        if (element.Name == "Chassis_M46_Patton")
        {
            lengthInBytes = lengthInBytes;
        }
        return dataDescriptor.end;
    }

    public void WriteElement(BinaryWriter writer,XmlNode element, XmlDocument xDoc, List<string> dictionary)
    {
        WriteLittleEndianShort(writer, (short)element.ChildNodes.Count); //childrenNumber

    }

    public void readElement(BinaryReader reader, XmlNode element, XmlDocument xDoc, List<string> dictionary)
    {
        //pos204
        int childrenNmber = readLittleEndianShort(reader);
        DataDescriptor selfDataDescriptor = readDataDescriptor(reader);
        
        ElementDescriptor[] children = readElementDescriptors(reader, childrenNmber);
        
        int offset = readData(reader, dictionary, element, xDoc, 0, selfDataDescriptor);

        foreach (ElementDescriptor elementDescriptor in children)
        {
            var elementName = dictionary[elementDescriptor.nameIndex];

            //Console.WriteLine(elementName);
            
            XmlNode child = xDoc.CreateElement(elementName);
            offset = readData(reader, dictionary, child, xDoc, offset, elementDescriptor.dataDescriptor);
            element.AppendChild(child);
        }
    }
}


// =======================================================
// Service provided by Telerik (www.telerik.com)
// Conversion powered by NRefactory.
// Twitter: @telerik, @toddanglin
// Facebook: facebook.com/telerik
// =======================================================
