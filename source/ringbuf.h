#pragma once
#include "zix/ring.h"
#include <stdexcept>
#include <vector>
#include <array>

namespace ringbuf
{
    class PacketBase
    {
    public:
        PacketBase() = default;
        PacketBase(PacketBase&&) = default;
        PacketBase& operator=(PacketBase&&) = default;
        virtual ~PacketBase() = default;
    };
    class RingBuf
    {
    public:
        RingBuf(uint32_t capacity, uint32_t maxpacketsize) : m_Capacity(capacity), m_MaxPacketSize(maxpacketsize)
        {
            if(Capacity() < 8)
            {
                throw std::runtime_error("capacity < 8");
            }
            if(Capacity() < MaxPacketSize() + 2 * sizeof(uint32_t))
            {
                throw std::runtime_error("capacity < maxpacketsize");
            }
            m_Ring = zix_ring_new(nullptr, capacity);
            zix_ring_mlock(m_Ring);
            m_ReadBuffer.resize(MaxPacketSize());
        }
        ~RingBuf()
        {
            zix_ring_free(m_Ring);
        }
        uint32_t MaxPacketSize() const
        {
            return m_MaxPacketSize;
        }
        uint32_t Capacity() const
        {
            return m_Capacity;
        }
        template<class T>
        bool Write(const T& packet, bool throwiffail = true)
        {
            static_assert(std::is_base_of<PacketBase, T>::value, "T must descend from PacketBase");
            static_assert(std::is_trivially_destructible<T>::value, "T must have trivial destructor");
            static_assert(sizeof(T) )
            auto packetbaseptr = (const PacketBase*)&packet;
            ptrdiff_t packetbaseoffset =  (const char*)packetbaseptr - (const char*)&packet;
            if( (packetbaseoffset < 0)
            {
                throw std::runtime_error("packetbaseoffset < 0");
            }
            auto writesize = sizeof(T) + 2 * sizeof(uint32_t);
            if(writesize > MaxPacketSize())
            {
                throw std::runtime_error("packet larger than  MaxPacketSize");
            }
            if(writesize >  zix_ring_write_space(m_Ring))
            {
                if(throwiffail)
                {
                    throw std::runtime_error("ring full");
                }
                return false;
            }
            std::array<uint32_t, 2> header = {
                (uint32_t)sizeof(T),
                (uint32_t)packetbaseoffset
            };
            auto transaction = zix_ring_begin_write(m_Ring);
            zix_ring_amend_write(m_Ring, &transaction, header.data(), header.size() * sizeof(uint32_t));
            zix_ring_amend_write(m_Ring, &transaction, &packet, sizeof(T));
            zix_ring_commit_write(m_Ring, &transaction);
            return true;
        }
        const PacketBase* Read()
        {
            if(zix_ring_read_space(m_Ring) < 2 * sizeof(uint32_t))
            {
                return nullptr;
            }
            std::array<uint32_t, 2> header;
            zix_ring_read(m_Ring, header.data(), header.size() * sizeof(uint32_t));
            if(zix_ring_read_space(m_Ring) < header[0])
            {
                throw std::runtime_error("packet trunctated?!");
            }
            zix_ring_read(m_Ring, m_ReadBuffer.data(), header[0]);
            auto packetbaseptr = (const PacketBase*)(m_ReadBuffer.data() + header[1]);
            return packetbaseptr;
        }
    private:
        ZixRing* m_Ring = nullptr;
        uint32_t m_Capacity = 0;
        uint32_t m_MaxPacketSize = 0;
        std::vector<char> m_ReadBuffer;
    };
}