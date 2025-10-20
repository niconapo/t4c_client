#include "SmoothTileLoader.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include <assert.h>

namespace
{
   std::string BuildJsonPath(const std::string& rPathName)
   {
      std::string JsonPath = rPathName;

      std::string::size_type LastSlash = JsonPath.find_last_of("/\\");
      std::string::size_type DotPos = JsonPath.find_last_of('.');
      if (DotPos != std::string::npos && (LastSlash == std::string::npos || DotPos > LastSlash))
      {
         JsonPath.erase(DotPos);
      }
      JsonPath += ".json";

      return JsonPath;
   }

   void WriteUnsignedShortVector(std::ofstream& rStream, const std::vector<unsigned short>& rValues)
   {
      for (size_t i = 0; i < rValues.size(); ++i)
      {
         if (i != 0)
         {
            rStream << ", ";
         }
         rStream << rValues[i];
      }
   }

   void WriteSignature(std::ofstream& rStream, const std::vector<unsigned short>& rTiles)
   {
      const size_t SignatureSize = (rTiles.size() > 0) ? (rTiles.size() - 1) : 0;
      for (size_t i = 0; i < SignatureSize; ++i)
      {
         if (i != 0)
         {
            rStream << ", ";
         }
         rStream << rTiles[i];
      }
   }

   void WriteOffsets(std::ofstream& rStream, const std::vector<SmoothTile::TileDefinition::TileOffsets>& rOffsets)
   {
      for (size_t i = 0; i < rOffsets.size(); ++i)
      {
         if (i != 0)
         {
            rStream << ", ";
         }
         rStream << "{\"x\": " << static_cast<int>(rOffsets[i].m_OffsetX)
                 << ", \"y\": " << static_cast<int>(rOffsets[i].m_OffsetY) << "}";
      }
   }

   void WriteDefinitionMapping(std::ofstream& rStream, const std::map<int, unsigned short>& rMapping)
   {
      std::map<int, unsigned short>::const_iterator itCur = rMapping.begin();
      std::map<int, unsigned short>::const_iterator itEnd = rMapping.end();
      for (; itCur != itEnd; ++itCur)
      {
         if (itCur != rMapping.begin())
         {
            rStream << ", ";
         }
         rStream << "\"" << itCur->first << "\": " << itCur->second;
      }
   }

   struct SortTilesByID
   {
      bool operator()(const std::pair<const SmoothTile::TileDefinition*, const SmoothTile*>& rLeft,
                      const std::pair<const SmoothTile::TileDefinition*, const SmoothTile*>& rRight) const
      {
         return rLeft.second->m_ID < rRight.second->m_ID;
      }
   };

   bool SaveTileBehaviorsJson(const std::string& rPathName,
                              const std::map<SmoothTile::TileDefinition, SmoothTile>& rMapTiles)
   {
      const std::string JsonPath = BuildJsonPath(rPathName);

      std::ofstream JsonFile(JsonPath.c_str());
      if (!JsonFile.is_open())
      {
         std::cerr << "Unable to open tile behavior JSON file: " << JsonPath << std::endl;
         return false;
      }

      const std::map<SmoothTile::TileDefinition, SmoothTile>& MapRef = rMapTiles;
      typedef std::map<SmoothTile::TileDefinition, SmoothTile>::const_iterator ConstIterator;

      std::vector<std::pair<const SmoothTile::TileDefinition*, const SmoothTile*> > SortedTiles;
      SortedTiles.reserve(MapRef.size());
      for (ConstIterator itCur = MapRef.begin(); itCur != MapRef.end(); ++itCur)
      {
         SortedTiles.push_back(std::make_pair(&itCur->first, &itCur->second));
      }

      std::sort(SortedTiles.begin(), SortedTiles.end(), SortTilesByID());

      JsonFile << "{\n";
      JsonFile << "  \"tiles\": [\n";

      for (size_t i = 0; i < SortedTiles.size(); ++i)
      {
         const SmoothTile::TileDefinition& Definition = *SortedTiles[i].first;
         const SmoothTile& Tile = *SortedTiles[i].second;

         if (i != 0)
         {
            JsonFile << ",\n";
         }

         JsonFile << "    {\n";
         JsonFile << "      \"id\": " << Tile.m_ID << ",\n";
         JsonFile << "      \"signature\": [";
         WriteSignature(JsonFile, Definition.m_Tiles);
         JsonFile << "],\n";
         JsonFile << "      \"smooth_size\": "
                  << (Definition.m_Tiles.empty() ? 0 : Definition.m_Tiles.back()) << ",\n";
         JsonFile << "      \"colors\": [";
         WriteUnsignedShortVector(JsonFile, Definition.m_Colors);
         JsonFile << "],\n";
         JsonFile << "      \"offsets\": [";
         WriteOffsets(JsonFile, Definition.m_TilesOffset);
         JsonFile << "],\n";
         JsonFile << "      \"definition_to_tile\": {";
         WriteDefinitionMapping(JsonFile, Definition.m_MapDefinitionIDToTileID);
         JsonFile << "}\n";
         JsonFile << "    }";
      }

      JsonFile << "\n  ]\n";
      JsonFile << "}\n";

      if (!JsonFile.good())
      {
         std::cerr << "Failed while writing tile behavior JSON file: " << JsonPath << std::endl;
         return false;
      }

      return true;
   }
}

SmoothTileLoader::SmoothTileLoader(void)
{
}

SmoothTileLoader::~SmoothTileLoader(void)
{
}

int SmoothTileLoader::LoadExceptionsFromFile(const std::string& rPathName,
                                             std::set<unsigned short>& rSetExceptions)
{
   rSetExceptions.clear();

   std::ifstream InputFile;
   InputFile.open(rPathName.c_str());

   unsigned short Exception = 0;
   InputFile >> Exception;
   while (!InputFile.eof())
   {
      rSetExceptions.insert(Exception);
      InputFile >> Exception;
   }

   return 0;
}

int SmoothTileLoader::LoadFromFile(const std::string& rPathName,
                                   std::map<SmoothTile::TileDefinition, SmoothTile>& rMapTiles)
{
   rMapTiles.clear();

   std::ifstream InputFile;
   InputFile.open(rPathName.c_str(), std::ios::binary);

   char Header[4] = { 0 };
   InputFile.read(Header, sizeof(Header));
   unsigned short Version;
   InputFile.read(reinterpret_cast<char*>(&Version), sizeof(Version));

   unsigned short Count = 0;
   InputFile.read(reinterpret_cast<char*>(&Count), sizeof(Count));

   for (int i = 0; i < Count; ++i)
   {
      SmoothTile::TileDefinition TileDef;
      TileDef.Load(InputFile);
      rMapTiles[TileDef].Load(InputFile);
   }

   return 0;
}

int SmoothTileLoader::SaveToFile(const std::string& rPathName,
                                 std::map<SmoothTile::TileDefinition, SmoothTile>& rMapTiles)
{
   std::ofstream OutputFile;
   OutputFile.open(rPathName.c_str(), std::ios::binary);
   if (!OutputFile.is_open())
   {
      std::cerr << "Unable to open smooth tile file for writing: " << rPathName << std::endl;
      return -1;
   }

   char Header[4] = { 'S', 'T', 'S', 0};
   unsigned short Version = 0x0100;
   OutputFile.write(Header, sizeof(Header));
   OutputFile.write(reinterpret_cast<char*>(&Version), sizeof(Version));

   unsigned short Count = rMapTiles.size();
   OutputFile.write(reinterpret_cast<char*>(&Count), sizeof(Count));

   std::map<SmoothTile::TileDefinition, SmoothTile>::iterator itCur = rMapTiles.begin();
   std::map<SmoothTile::TileDefinition, SmoothTile>::iterator itEnd = rMapTiles.end();
   for (itCur; itCur != itEnd; ++itCur)
   {
      itCur->first.Save(OutputFile);
      itCur->second.Save(OutputFile);
   }

   OutputFile.flush();

   if (!OutputFile.good())
   {
      std::cerr << "Failed while writing smooth tile file: " << rPathName << std::endl;
      OutputFile.close();
      return -1;
   }

   OutputFile.close();

   const bool bJsonResult = SaveTileBehaviorsJson(rPathName, rMapTiles);

   return bJsonResult ? 0 : -1;
}

int SmoothTileLoader::MapToVector( const std::map<SmoothTile::TileDefinition, SmoothTile>& rMap, std::vector<SmoothTile>& rVector )
{
   std::vector<bool> VecLoaded;
   VecLoaded.resize(rMap.size(), false);
   rVector.clear();
   rVector.resize(rMap.size());
   std::map<SmoothTile::TileDefinition, SmoothTile>::const_iterator itCur = rMap.begin();
   std::map<SmoothTile::TileDefinition, SmoothTile>::const_iterator itEnd = rMap.end();

   for (itCur; itCur != itEnd; ++itCur)
   {
      assert(itCur->second.m_ID < rVector.size());
      //assert(VecLoaded[itCur->second.m_ID] == false);
      if (VecLoaded[itCur->second.m_ID] == true)
      {
         //std::cout << "Duplicate found: " << itCur->second.m_ID << std::endl;
      }
      VecLoaded[itCur->second.m_ID] = true;
      rVector[itCur->second.m_ID] = itCur->second;
   }

   return 0;
}

int SmoothTileLoader::MapToVector( const std::map<SmoothTile::TileDefinition, SmoothTile>& rMap, std::vector<SmoothTile::TileDefinition>& rVector )
{
   std::vector<bool> VecLoaded;
   VecLoaded.resize(rMap.size(), false);
   rVector.clear();
   rVector.resize(rMap.size());
   std::map<SmoothTile::TileDefinition, SmoothTile>::const_iterator itCur = rMap.begin();
   std::map<SmoothTile::TileDefinition, SmoothTile>::const_iterator itEnd = rMap.end();

   for (itCur; itCur != itEnd; ++itCur)
   {
      assert(itCur->second.m_ID < rVector.size());
      //assert(VecLoaded[itCur->second.m_ID] == false);
      if (VecLoaded[itCur->second.m_ID] == true)
      {
         //std::cout << "Duplicate found: " << itCur->second.m_ID << std::endl;
      }
      VecLoaded[itCur->second.m_ID] = true;
      rVector[itCur->second.m_ID] = itCur->first;
   }

   return 0;
}
