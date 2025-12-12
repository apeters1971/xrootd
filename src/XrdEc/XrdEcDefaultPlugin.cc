/*
 * XrdEcDefaultPlugin.cc
 *
 * Client-side EC plugin factory for XrdCl.
 * Builds in XrdEc to avoid cyclic linkage with XrdCl.
 */

#include "XrdCl/XrdClPlugInInterface.hh"
#include "XrdCl/XrdClUtils.hh"
#include "XrdCl/XrdClEcHandler.hh"
#include "XrdVersion.hh"

#include <memory>
#include <cstdlib>
#include <vector>
#include <string>
#include <map>

using namespace XrdCl;

namespace
{
  class EcDefaultFactory : public PlugInFactory
  {
    public:
      EcDefaultFactory( uint8_t nbdta,
                        uint8_t nbprt,
                        uint64_t chsz,
                        std::vector<std::string> &&plgr )
        : nbdta( nbdta ), nbprt( nbprt ), chsz( chsz ), plgr( std::move( plgr ) )
      {}

      FilePlugIn *CreateFile( const std::string &u ) override
      {
        URL url( u );
        // note: chsz is stripe size for data; ObjCfg ctor expects chunksize = blksize/nbdata
        auto *objcfg = new XrdEc::ObjCfg( url.GetPath(), nbdta, nbprt, chsz, false, true );
        objcfg->plgr = plgr;
        return new EcHandler( url, objcfg, nullptr );
      }

      FileSystemPlugIn *CreateFileSystem( const std::string & ) override
      {
        return nullptr;
      }

    private:
      uint8_t nbdta;
      uint8_t nbprt;
      uint64_t chsz;
      std::vector<std::string> plgr;
  };
}

extern "C"
{
  XrdVERSIONINFO(XrdClGetPlugIn, XrdClGetPlugIn)

  void *XrdClGetPlugIn( const void *arg )
  {
    const auto *cfg = static_cast<const std::map<std::string,std::string>*>( arg );

    uint8_t nbdta = 0, nbprt = 0;
    uint64_t chsz = 0;
    std::vector<std::string> plgr;

    auto parseCfg = [&](const std::string &key, uint64_t &out) -> bool {
      auto it = cfg ? cfg->find( key ) : cfg->end();
      if( it == cfg->end() ) return false;
      out = std::stoull( it->second );
      return true;
    };

    bool ok = true;
    if( cfg )
    {
      uint64_t tmp = 0;
      ok &= parseCfg( "nbdta", tmp ); nbdta = static_cast<uint8_t>( tmp );
      ok &= parseCfg( "nbprt", tmp ); nbprt = static_cast<uint8_t>( tmp );
      ok &= parseCfg( "chsz",  tmp );
      chsz = tmp;
      auto it = cfg->find( "plgr" );
      if( it != cfg->end() ) Utils::splitString( plgr, it->second, "," );
    }
    else
    {
      const char *env = std::getenv( "XRDCL_EC" );
      if( !env ) ok = false;
      else
      {
        // expect nbdta,nbprt,chsz
        std::vector<std::string> parts;
        Utils::splitString( parts, env, "," );
        if( parts.size() >= 3 )
        {
          nbdta = static_cast<uint8_t>( std::stoul( parts[0] ) );
          nbprt = static_cast<uint8_t>( std::stoul( parts[1] ) );
          chsz  = std::stoull( parts[2] );
        }
        else ok = false;
      }
    }

    if( !ok || nbdta == 0 ) return nullptr;

    return static_cast<void*>( new EcDefaultFactory( nbdta, nbprt, chsz, std::move( plgr ) ) );
  }
}

