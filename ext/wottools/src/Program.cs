using System;
using System.IO;
 
using Ptwr.PackedXml; 

// sourced and adapted from https://github.com/PTwr/WotTools

class Program {
    static int Main(string[] args)
    {
        if (TryDecodeXml(args[0], Console.OpenStandardInput(), out var decodedXml, true, null))
        {
            Console.WriteLine(decodedXml);

            return 0;
        }
        else
        {
            throw new Exception("Failed to decode xml");
        }

    }

    static bool TryDecodeXml(string path, Stream stream, out string decodedXml, bool verbose = true, string rootName = null)
    {
        decodedXml = "";
        rootName = rootName ?? Path.GetFileName(path);

        var memoryStream = new MemoryStream();
        stream.CopyTo(memoryStream);
        memoryStream.Seek(0, SeekOrigin.Begin);
        var binaryReader = new BinaryReader(memoryStream);
        var head = binaryReader.ReadInt32();

        if (head != Packed_Section.Packed_Header)
        {
            return verbose ? throw new InvalidDataException("File is not packed xml") : false;
        }

        string xml = Ptwr.PackedXml.Reader.DecodePackedFile(binaryReader, rootName);

        decodedXml = xml;

        return !string.IsNullOrWhiteSpace(xml);
    }
}


