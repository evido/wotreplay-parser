using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml;

// sourced from https://github.com/PTwr/WotTools

namespace Ptwr.PackedXml
{
    public class Reader
    {
        public static string DecodePackedFile(BinaryReader reader, string rootName)
        {
            XmlDocument xDoc = new XmlDocument();

            var sb = reader.ReadSByte();

            var PS = new Packed_Section();

            var dictionary = PS.readDictionary(reader);

            var xmlroot = xDoc.CreateNode(XmlNodeType.Element, rootName, "");
            //pos202
            PS.readElement(reader, xmlroot, xDoc, dictionary);

            return xmlroot.OuterXml;
        }

        public static List<string> GetXmlDict(XmlNode element)
        {
            if (element.NodeType != XmlNodeType.Element)
            {
                return new List<string>();
            }

            List<string> result = new List<string>();
            if (element.Name == "xmlref")
            {
                result.Add("xmlns:xmlref");
                //Debugger.Break();
            }
            else
            {
                result.Add(element.Name);
            }
            foreach (XmlNode node in element.ChildNodes)
            {
                result.AddRange(GetXmlDict(node));
            }
            return result;
        }
        public static List<string> GetXmlDict(XmlDocument doc)
        {
            List<string> result = new List<string>();
            foreach (XmlNode node in doc.FirstChild.ChildNodes)
            {
                result.AddRange(GetXmlDict(node));
            }

            result = result.Distinct().OrderBy(i => i).ToList();

            return result;
        }
        public static void EncodePackedFile(string path, string xml)
        {
            var PS = new Packed_Section();

            XmlDocument xDoc = new XmlDocument();
            xDoc.LoadXml(xml);

            File.Delete(path);
            var f = new FileStream(path, FileMode.OpenOrCreate, FileAccess.Write);
            BinaryWriter writer = new BinaryWriter(f);

            //header
            writer.Write(Packed_Section.Packed_Header);
            writer.Write((sbyte)0);

            //dictionary
            var newDict = GetXmlDict(xDoc);
            PS.writeDictionary(writer, newDict);

            //records
            PS.WriteElement(writer, xDoc.FirstChild, xDoc, newDict);

            f.Flush();
            f.Close();
        }
        public static void EncodePackedFile(string xml, out byte[] bytes)
        {
            var PS = new Packed_Section();

            XmlDocument xDoc = new XmlDocument();
            xDoc.LoadXml(xml);

            var f = new MemoryStream();
            BinaryWriter writer = new BinaryWriter(f);

            //header
            writer.Write(Packed_Section.Packed_Header);
            writer.Write((sbyte)0);

            //dictionary
            var newDict = GetXmlDict(xDoc);
            PS.writeDictionary(writer, newDict);

            //records
            PS.WriteElement(writer, xDoc.FirstChild, xDoc, newDict);

            f.Flush();

            bytes = f.ToArray();

            f.Close();
        }
    }
}
