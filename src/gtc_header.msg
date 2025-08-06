//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
//

packet gtc_header extends cPacket
{
    bool Downlink = false;
    bool Uplink = false;
    
    //uint32_t PSync;					// 4 Bytes
    //uint32_t Ident;					// 4 Bytes
    //double PLOAMd;					// 13 Bytes
    //uint8_t BIP;						// 1 Byte
    //uint32_t Plend;					// 4 Bytes (repeated twice)
    //double us_bw_map_size;			// N*8 Bytes
    
    double Olt_onu_rtt[];
    double Onu_start_time_TC1[];
    double Onu_grant_TC1[];
    double Onu_start_time_TC2[];
    double Onu_grant_TC2[];
    double Onu_start_time_TC3[];
    double Onu_grant_TC3[];
    
    //uint8_t AllocID;					// 14 bits
    //uint8_t Flags;					// 2 bits
    //uint8_t StartTime;				// 16 bits
    //uint8_t GrantSize;				// 16 bits
    //uint8_t FWI;						// 1 bit
    //uint8_t BurstProfile;				// 2 bits
    //uint8_t HEC;						// 13 bits
    
    int OnuID;
    double BufferOccupancyTC1 = 0;				// 24 bits
    double BufferOccupancyTC2 = 0;				// 24 bits
    double BufferOccupancyTC3 = 0;				// 24 bits
    
    long SeqID;
    
    // int flags;						// 8 bits
    // int allocID;						// 12 bits
    // double timestamp;				// 20 bits - Local time value used for ranging and drift correction
}




