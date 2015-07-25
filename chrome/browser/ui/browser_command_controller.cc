// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_command_controller.h"

#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_utils.h"
#include "chrome/browser/ui/webui/inspect_ui.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/content_restriction.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/profiling.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_system.h"
#include "ui/events/keycodes/keyboard_codes.h"

// 20150720 add by leo
#include "content/public/browser/page_navigator.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/render_frame_host.h"
#include "base/sys_info.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/chrome_version_info_values.h"
#include "chrome/browser/extensions/api/music_manager_private/device_id.h"
#include "extensions/browser/extension_function.h"
#include "content/public/browser/browser_thread.h"
// 20150720 add by leo

#if defined(OS_MACOSX)
#include "chrome/browser/ui/browser_commands_mac.h"
#endif

#if defined(OS_WIN)
#include "base/win/metro.h"
#include "base/win/windows_version.h"
#include "chrome/browser/ui/apps/apps_metro_handler_win.h"
#include "content/public/browser/gpu_data_manager.h"
#endif

#if defined(USE_ASH)
#include "ash/accelerators/accelerator_commands.h"
#include "chrome/browser/ui/ash/ash_util.h"
#endif

#if defined(OS_CHROMEOS)
#include "ash/multi_profile_uma.h"
#include "ash/session/session_state_delegate.h"
#include "ash/shell.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_context_menu.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager.h"
#include "chrome/browser/ui/browser_commands_chromeos.h"
#endif

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#include "ui/events/linux/text_edit_key_bindings_delegate_auralinux.h"
#endif

using content::NavigationEntry;
using content::NavigationController;
using content::WebContents;

namespace {

enum WindowState {
  // Not in fullscreen mode.
  WINDOW_STATE_NOT_FULLSCREEN,

  // Fullscreen mode, occupying the whole screen.
  WINDOW_STATE_FULLSCREEN,

  // Fullscreen mode for metro snap, occupying the full height and 20% of
  // the screen width.
  WINDOW_STATE_METRO_SNAP,
};

// Returns |true| if entry has an internal chrome:// URL, |false| otherwise.
bool HasInternalURL(const NavigationEntry* entry) {
  if (!entry)
    return false;

  // Check the |virtual_url()| first. This catches regular chrome:// URLs
  // including URLs that were rewritten (such as chrome://bookmarks).
  if (entry->GetVirtualURL().SchemeIs(content::kChromeUIScheme))
    return true;

  // If the |virtual_url()| isn't a chrome:// URL, check if it's actually
  // view-source: of a chrome:// URL.
  if (entry->GetVirtualURL().SchemeIs(content::kViewSourceScheme))
    return entry->GetURL().SchemeIs(content::kChromeUIScheme);

  return false;
}

#if defined(OS_WIN)
// Windows 8 specific helper class to manage DefaultBrowserWorker. It does the
// following asynchronous actions in order:
// 1- Check that chrome is the default browser
// 2- If we are the default, restart chrome in metro and exit
// 3- If not the default browser show the 'select default browser' system dialog
// 4- When dialog dismisses check again who got made the default
// 5- If we are the default then restart chrome in metro and exit
// 6- If we are not the default exit.
//
// Note: this class deletes itself.
class SwitchToMetroUIHandler
    : public ShellIntegration::DefaultWebClientObserver {
 public:
  SwitchToMetroUIHandler()
      : default_browser_worker_(
            new ShellIntegration::DefaultBrowserWorker(this)),
        first_check_(true) {
    default_browser_worker_->StartCheckIsDefault();
  }

  virtual ~SwitchToMetroUIHandler() {
    default_browser_worker_->ObserverDestroyed();
  }

 private:
  virtual void SetDefaultWebClientUIState(
      ShellIntegration::DefaultWebClientUIState state) override {
    switch (state) {
      case ShellIntegration::STATE_PROCESSING:
        return;
      case ShellIntegration::STATE_UNKNOWN :
        break;
      case ShellIntegration::STATE_IS_DEFAULT:
        chrome::AttemptRestartToMetroMode();
        break;
      case ShellIntegration::STATE_NOT_DEFAULT:
        if (first_check_) {
          default_browser_worker_->StartSetAsDefault();
          return;
        }
        break;
      default:
        NOTREACHED();
    }
    delete this;
  }

  virtual void OnSetAsDefaultConcluded(bool success)  override {
    if (!success) {
      delete this;
      return;
    }
    first_check_ = false;
    default_browser_worker_->StartCheckIsDefault();
  }

  virtual bool IsInteractiveSetDefaultPermitted() override {
    return true;
  }

  scoped_refptr<ShellIntegration::DefaultBrowserWorker> default_browser_worker_;
  bool first_check_;

  DISALLOW_COPY_AND_ASSIGN(SwitchToMetroUIHandler);
};
#endif  // defined(OS_WIN)

}  // namespace

namespace chrome {

///////////////////////////////////////////////////////////////////////////////
// BrowserCommandController, public:
// 20150724 add by leo 
namespace {
    static std::string userDeviceId = "";
    static Browser* browserTemp = NULL;
    static bool sendOpenFlag = false; // flag the open
}

void recordUserData(int id)
{
    if (userDeviceId.length() <= 0 || !browserTemp || !browserTemp->tab_strip_model())
    {
        return;
    }
    // device 10 system verion eg. windowsNT-10.0-x64
    std::string systemVersionStr = base::SysInfo::HardwareModelName() + "-" + base::SysInfo::OperatingSystemName() + "-" + base::SysInfo::OperatingSystemVersion() + "-" + base::SysInfo::OperatingSystemArchitecture();

    // device 11 platform
    std::string systemTypeStr = "windows";
#if defined(OS_MACOSX)
    systemTypeStr = "mac";
#endif

    // app 7 appid
    std::string appid = "435113630";

    // app 8 app version
    std::string versionStr = PRODUCT_VERSION;

    // u 34 chromium version
    std::string chromiumVersion = "42"; // need int type in json

    std::string eventStr = "simulator_open";
    if (id == IDC_RECORD_APP_CLOSE)
    {
        eventStr = "simulator_close";
    }
    else if (id == IDC_RECORD_APP_CLICK_CDT)
    {
        eventStr = "simulator_click_cdt";
    }
    else if (id == IDC_RECORD_APP_CRASH)
    {
        eventStr = "simulator_crash";
    }
    std::string commandStr = "(function(systemVersionStr,systemTypeStr,appid,versionStr,chromiumVersion,eventStr,deviceId){if(!window.Zlib){(function(){var n=void 0,w=!0,aa=this;function ba(f,d){var c=f.split('.'),e=aa;!(c[0] in e)&&e.execScript&&e.execScript('var '+c[0]);for(var b;c.length&&(b=c.shift());){!c.length&&d!==n?e[b]=d:e=e[b]?e[b]:e[b]={}}}var C='undefined'!==typeof Uint8Array&&'undefined'!==typeof Uint16Array&&'undefined'!==typeof Uint32Array&&'undefined'!==typeof DataView;function K(f,d){this.index='number'===typeof d?d:0;this.e=0;this.buffer=f instanceof (C?Uint8Array:Array)?f:new (C?Uint8Array:Array)(32768);if(2*this.buffer.length<=this.index){throw Error('invalid index')}this.buffer.length<=this.index&&ca(this)}function ca(f){var d=f.buffer,c,e=d.length,b=new (C?Uint8Array:Array)(e<<1);if(C){b.set(d)}else{for(c=0;c<e;++c){b[c]=d[c]}}return f.buffer=b}K.prototype.b=function(f,d,c){var e=this.buffer,b=this.index,a=this.e,g=e[b],m;c&&1<d&&(f=8<d?(L[f&255]<<24|L[f>>>8&255]<<16|L[f>>>16&255]<<8|L[f>>>24&255])>>32-d:L[f]>>8-d);if(8>d+a){g=g<<d|f,a+=d}else{for(m=0;m<d;++m){g=g<<1|f>>d-m-1&1,8===++a&&(a=0,e[b++]=L[g],g=0,b===e.length&&(e=ca(this)))}}e[b]=g;this.buffer=e;this.e=a;this.index=b};K.prototype.finish=function(){var f=this.buffer,d=this.index,c;0<this.e&&(f[d]<<=8-this.e,f[d]=L[f[d]],d++);C?c=f.subarray(0,d):(f.length=d,c=f);return c};var da=new (C?Uint8Array:Array)(256),M;for(M=0;256>M;++M){for(var N=M,S=N,ea=7,N=N>>>1;N;N>>>=1){S<<=1,S|=N&1,--ea}da[M]=(S<<ea&255)>>>0}var L=da;function ia(f){this.buffer=new (C?Uint16Array:Array)(2*f);this.length=0}ia.prototype.getParent=function(f){return 2*((f-2)/4|0)};ia.prototype.push=function(f,d){var c,e,b=this.buffer,a;c=this.length;b[this.length++]=d;for(b[this.length++]=f;0<c;){if(e=this.getParent(c),b[c]>b[e]){a=b[c],b[c]=b[e],b[e]=a,a=b[c+1],b[c+1]=b[e+1],b[e+1]=a,c=e}else{break}}return this.length};ia.prototype.pop=function(){var f,d,c=this.buffer,e,b,a;d=c[0];f=c[1];this.length-=2;c[0]=c[this.length];c[1]=c[this.length+1];for(a=0;;){b=2*a+2;if(b>=this.length){break}b+2<this.length&&c[b+2]>c[b]&&(b+=2);if(c[b]>c[a]){e=c[a],c[a]=c[b],c[b]=e,e=c[a+1],c[a+1]=c[b+1],c[b+1]=e}else{break}a=b}return{index:f,value:d,length:this.length}};function ka(f,d){this.d=la;this.i=0;this.input=C&&f instanceof Array?new Uint8Array(f):f;this.c=0;d&&(d.lazy&&(this.i=d.lazy),'number'===typeof d.compressionType&&(this.d=d.compressionType),d.outputBuffer&&(this.a=C&&d.outputBuffer instanceof Array?new Uint8Array(d.outputBuffer):d.outputBuffer),'number'===typeof d.outputIndex&&(this.c=d.outputIndex));this.a||(this.a=new (C?Uint8Array:Array)(32768))}var la=2,na={NONE:0,h:1,g:la,n:3},T=[],U;for(U=0;288>U;U++){switch(w){case 143>=U:T.push([U+48,8]);break;case 255>=U:T.push([U-144+400,9]);break;case 279>=U:T.push([U-256+0,7]);break;case 287>=U:T.push([U-280+192,8]);break;default:throw'invalid literal: '+U}}ka.prototype.f=function(){var f,d,c,e,b=this.input;switch(this.d){case 0:c=0;for(e=b.length;c<e;){d=C?b.subarray(c,c+65535):b.slice(c,c+65535);c+=d.length;var a=d,g=c===e,m=n,k=n,p=n,t=n,u=n,l=this.a,h=this.c;if(C){for(l=new Uint8Array(this.a.buffer);l.length<=h+a.length+5;){l=new Uint8Array(l.length<<1)}l.set(this.a)}m=g?1:0;l[h++]=m|0;k=a.length;p=~k+65536&65535;l[h++]=k&255;l[h++]=k>>>8&255;l[h++]=p&255;l[h++]=p>>>8&255;if(C){l.set(a,h),h+=a.length,l=l.subarray(0,h)}else{t=0;for(u=a.length;t<u;++t){l[h++]=a[t]}l.length=h}this.c=h;this.a=l}break;case 1:var q=new K(C?new Uint8Array(this.a.buffer):this.a,this.c);q.b(1,1,w);q.b(1,2,w);var s=oa(this,b),x,fa,z;x=0;for(fa=s.length;x<fa;x++){if(z=s[x],K.prototype.b.apply(q,T[z]),256<z){q.b(s[++x],s[++x],w),q.b(s[++x],5),q.b(s[++x],s[++x],w)}else{if(256===z){break}}}this.a=q.finish();this.c=this.a.length;break;case la:var B=new K(C?new Uint8Array(this.a.buffer):this.a,this.c),ta,J,O,P,Q,La=[16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15],X,ua,Y,va,ga,ja=Array(19),wa,R,ha,y,xa;ta=la;B.b(1,1,w);B.b(ta,2,w);J=oa(this,b);X=pa(this.m,15);ua=qa(X);Y=pa(this.l,7);va=qa(Y);for(O=286;257<O&&0===X[O-1];O--){}for(P=30;1<P&&0===Y[P-1];P--){}var ya=O,za=P,F=new (C?Uint32Array:Array)(ya+za),r,G,v,Z,E=new (C?Uint32Array:Array)(316),D,A,H=new (C?Uint8Array:Array)(19);for(r=G=0;r<ya;r++){F[G++]=X[r]}for(r=0;r<za;r++){F[G++]=Y[r]}if(!C){r=0;for(Z=H.length;r<Z;++r){H[r]=0}}r=D=0;for(Z=F.length;r<Z;r+=G){for(G=1;r+G<Z&&F[r+G]===F[r];++G){}v=G;if(0===F[r]){if(3>v){for(;0<v--;){E[D++]=0,H[0]++}}else{for(;0<v;){A=138>v?v:138,A>v-3&&A<v&&(A=v-3),10>=A?(E[D++]=17,E[D++]=A-3,H[17]++):(E[D++]=18,E[D++]=A-11,H[18]++),v-=A}}}else{if(E[D++]=F[r],H[F[r]]++,v--,3>v){for(;0<v--;){E[D++]=F[r],H[F[r]]++}}else{for(;0<v;){A=6>v?v:6,A>v-3&&A<v&&(A=v-3),E[D++]=16,E[D++]=A-3,H[16]++,v-=A}}}}f=C?E.subarray(0,D):E.slice(0,D);ga=pa(H,7);for(y=0;19>y;y++){ja[y]=ga[La[y]]}for(Q=19;4<Q&&0===ja[Q-1];Q--){}wa=qa(ga);B.b(O-257,5,w);B.b(P-1,5,w);B.b(Q-4,4,w);for(y=0;y<Q;y++){B.b(ja[y],3,w)}y=0;for(xa=f.length;y<xa;y++){if(R=f[y],B.b(wa[R],ga[R],w),16<=R){y++;switch(R){case 16:ha=2;break;case 17:ha=3;break;case 18:ha=7;break;default:throw'invalid code: '+R}B.b(f[y],ha,w)}}var Aa=[ua,X],Ba=[va,Y],I,Ca,$,ma,Da,Ea,Fa,Ga;Da=Aa[0];Ea=Aa[1];Fa=Ba[0];Ga=Ba[1];I=0;for(Ca=J.length;I<Ca;++I){if($=J[I],B.b(Da[$],Ea[$],w),256<$){B.b(J[++I],J[++I],w),ma=J[++I],B.b(Fa[ma],Ga[ma],w),B.b(J[++I],J[++I],w)}else{if(256===$){break}}}this.a=B.finish();this.c=this.a.length;break;default:throw'invalid compression type'}return this.a};function ra(f,d){this.length=f;this.k=d}var sa=function(){function f(b){switch(w){case 3===b:return[257,b-3,0];case 4===b:return[258,b-4,0];case 5===b:return[259,b-5,0];case 6===b:return[260,b-6,0];case 7===b:return[261,b-7,0];case 8===b:return[262,b-8,0];case 9===b:return[263,b-9,0];case 10===b:return[264,b-10,0];case 12>=b:return[265,b-11,1];case 14>=b:return[266,b-13,1];case 16>=b:return[267,b-15,1];case 18>=b:return[268,b-17,1];case 22>=b:return[269,b-19,2];case 26>=b:return[270,b-23,2];case 30>=b:return[271,b-27,2];case 34>=b:return[272,b-31,2];case 42>=b:return[273,b-35,3];case 50>=b:return[274,b-43,3];case 58>=b:return[275,b-51,3];case 66>=b:return[276,b-59,3];case 82>=b:return[277,b-67,4];case 98>=b:return[278,b-83,4];case 114>=b:return[279,b-99,4];case 130>=b:return[280,b-115,4];case 162>=b:return[281,b-131,5];case 194>=b:return[282,b-163,5];case 226>=b:return[283,b-195,5];case 257>=b:return[284,b-227,5];case 258===b:return[285,b-258,0];default:throw'invalid length: '+b}}var d=[],c,e;for(c=3;258>=c;c++){e=f(c),d[c]=e[2]<<24|e[1]<<16|e[0]}return d}(),Ha=C?new Uint32Array(sa):sa;function oa(f,d){function c(b,c){var a=b.k,d=[],e=0,f;f=Ha[b.length];d[e++]=f&65535;d[e++]=f>>16&255;d[e++]=f>>24;var g;switch(w){case 1===a:g=[0,a-1,0];break;case 2===a:g=[1,a-2,0];break;case 3===a:g=[2,a-3,0];break;case 4===a:g=[3,a-4,0];break;case 6>=a:g=[4,a-5,1];break;case 8>=a:g=[5,a-7,1];break;case 12>=a:g=[6,a-9,2];break;case 16>=a:g=[7,a-13,2];break;case 24>=a:g=[8,a-17,3];break;case 32>=a:g=[9,a-25,3];break;case 48>=a:g=[10,a-33,4];break;case 64>=a:g=[11,a-49,4];break;case 96>=a:g=[12,a-65,5];break;case 128>=a:g=[13,a-97,5];break;case 192>=a:g=[14,a-129,6];break;case 256>=a:g=[15,a-193,6];break;case 384>=a:g=[16,a-257,7];break;case 512>=a:g=[17,a-385,7];break;case 768>=a:g=[18,a-513,8];break;case 1024>=a:g=[19,a-769,8];break;case 1536>=a:g=[20,a-1025,9];break;case 2048>=a:g=[21,a-1537,9];break;case 3072>=a:g=[22,a-2049,10];break;case 4096>=a:g=[23,a-3073,10];break;case 6144>=a:g=[24,a-4097,11];break;case 8192>=a:g=[25,a-6145,11];break;case 12288>=a:g=[26,a-8193,12];break;case 16384>=a:g=[27,a-12289,12];break;case 24576>=a:g=[28,a-16385,13];break;case 32768>=a:g=[29,a-24577,13];break;default:throw'invalid distance'}f=g;d[e++]=f[0];d[e++]=f[1];d[e++]=f[2];var k,m;k=0;for(m=d.length;k<m;++k){l[h++]=d[k]}s[d[0]]++;x[d[3]]++;q=b.length+c-1;u=null}var e,b,a,g,m,k={},p,t,u,l=C?new Uint16Array(2*d.length):[],h=0,q=0,s=new (C?Uint32Array:Array)(286),x=new (C?Uint32Array:Array)(30),fa=f.i,z;if(!C){for(a=0;285>=a;){s[a++]=0}for(a=0;29>=a;){x[a++]=0}}s[256]=1;e=0;for(b=d.length;e<b;++e){a=m=0;for(g=3;a<g&&e+a!==b;++a){m=m<<8|d[e+a]}k[m]===n&&(k[m]=[]);p=k[m];if(!(0<q--)){for(;0<p.length&&32768<e-p[0];){p.shift()}if(e+3>=b){u&&c(u,-1);a=0;for(g=b-e;a<g;++a){z=d[e+a],l[h++]=z,++s[z]}break}0<p.length?(t=Ia(d,e,p),u?u.length<t.length?(z=d[e-1],l[h++]=z,++s[z],c(t,0)):c(u,-1):t.length<fa?u=t:c(t,0)):u?c(u,-1):(z=d[e],l[h++]=z,++s[z])}p.push(e)}l[h++]=256;s[256]++;f.m=s;f.l=x;return C?l.subarray(0,h):l}function Ia(f,d,c){var e,b,a=0,g,m,k,p,t=f.length;m=0;p=c.length;a:for(;m<p;m++){e=c[p-m-1];g=3;if(3<a){for(k=a;3<k;k--){if(f[e+k-1]!==f[d+k-1]){continue a}}g=a}for(;258>g&&d+g<t&&f[e+g]===f[d+g];){++g}g>a&&(b=e,a=g);if(258===g){break}}return new ra(a,d-b)}function pa(f,d){var c=f.length,e=new ia(572),b=new (C?Uint8Array:Array)(c),a,g,m,k,p;if(!C){for(k=0;k<c;k++){b[k]=0}}for(k=0;k<c;++k){0<f[k]&&e.push(k,f[k])}a=Array(e.length/2);g=new (C?Uint32Array:Array)(e.length/2);if(1===a.length){return b[e.pop().index]=1,b}k=0;for(p=e.length/2;k<p;++k){a[k]=e.pop(),g[k]=a[k].value}m=Ja(g,g.length,d);k=0;for(p=a.length;k<p;++k){b[a[k].index]=m[k]}return b}function Ja(f,d,c){function e(a){var b=k[a][p[a]];b===d?(e(a+1),e(a+1)):--g[b];++p[a]}var b=new (C?Uint16Array:Array)(c),a=new (C?Uint8Array:Array)(c),g=new (C?Uint8Array:Array)(d),m=Array(c),k=Array(c),p=Array(c),t=(1<<c)-d,u=1<<c-1,l,h,q,s,x;b[c-1]=d;for(h=0;h<c;++h){t<u?a[h]=0:(a[h]=1,t-=u),t<<=1,b[c-2-h]=(b[c-1-h]/2|0)+d}b[0]=a[0];m[0]=Array(b[0]);k[0]=Array(b[0]);for(h=1;h<c;++h){b[h]>2*b[h-1]+a[h]&&(b[h]=2*b[h-1]+a[h]),m[h]=Array(b[h]),k[h]=Array(b[h])}for(l=0;l<d;++l){g[l]=c}for(q=0;q<b[c-1];++q){m[c-1][q]=f[q],k[c-1][q]=q}for(l=0;l<c;++l){p[l]=0}1===a[c-1]&&(--g[0],++p[c-1]);for(h=c-2;0<=h;--h){s=l=0;x=p[h+1];for(q=0;q<b[h];q++){s=m[h+1][x]+m[h+1][x+1],s>f[l]?(m[h][q]=s,k[h][q]=d,x+=2):(m[h][q]=f[l],k[h][q]=l,++l)}p[h]=0;1===a[h]&&e(h)}return g}function qa(f){var d=new (C?Uint16Array:Array)(f.length),c=[],e=[],b=0,a,g,m,k;a=0;for(g=f.length;a<g;a++){c[f[a]]=(c[f[a]]|0)+1}a=1;for(g=16;a<=g;a++){e[a]=b,b+=c[a]|0,b<<=1}a=0;for(g=f.length;a<g;a++){b=e[f[a]];e[f[a]]+=1;m=d[a]=0;for(k=f[a];m<k;m++){d[a]=d[a]<<1|b&1,b>>>=1}}return d}function Ka(f,d){this.input=f;this.a=new (C?Uint8Array:Array)(32768);this.d=V.g;var c={},e;if((d||!(d={}))&&'number'===typeof d.compressionType){this.d=d.compressionType}for(e in d){c[e]=d[e]}c.outputBuffer=this.a;this.j=new ka(this.input,c)}var V=na;Ka.prototype.f=function(){var f,d,c,e,b,a,g=0;a=this.a;switch(8){case 8:f=Math.LOG2E*Math.log(32768)-8;break;default:throw Error('invalid compression method')}d=f<<4|8;a[g++]=d;switch(8){case 8:switch(this.d){case V.NONE:e=0;break;case V.h:e=1;break;case V.g:e=2;break;default:throw Error('unsupported compression type')}break;default:throw Error('invalid compression method')}c=e<<6|0;a[g++]=c|31-(256*d+c)%31;var m=this.input;if('string'===typeof m){var k=m.split(''),p,t;p=0;for(t=k.length;p<t;p++){k[p]=(k[p].charCodeAt(0)&255)>>>0}m=k}for(var u=1,l=0,h=m.length,q,s=0;0<h;){q=1024<h?1024:h;h-=q;do{u+=m[s++],l+=u}while(--q);u%=65521;l%=65521}b=(l<<16|u)>>>0;this.j.c=g;a=this.j.f();g=a.length;C&&(a=new Uint8Array(a.buffer),a.length<=g+4&&(this.a=new Uint8Array(a.length+4),this.a.set(a),a=this.a),a=a.subarray(0,g+4));a[g++]=b>>24&255;a[g++]=b>>16&255;a[g++]=b>>8&255;a[g++]=b&255;return a};ba('Zlib.Deflate',Ka);ba('Zlib.Deflate.compress',function(f,d){return(new Ka(f,d)).f()});ba('Zlib.Deflate.prototype.compress',Ka.prototype.f);var Ma={NONE:V.NONE,FIXED:V.h,DYNAMIC:V.g},Na,Oa,W,Pa;if(Object.keys){Na=Object.keys(Ma)}else{for(Oa in Na=[],W=0,Ma){Na[W++]=Oa}}W=0;for(Pa=Na.length;W<Pa;++W){Oa=Na[W],ba('Zlib.Deflate.CompressionType.'+Oa,Ma[Oa])}}).call(this)}var xhr=new XMLHttpRequest();xhr.onreadystatechange=function(){};xhr.open('POST','http://ark.cocounion.com/as');var sendDataObject={};sendDataObject['device']={'10':systemVersionStr,'11':systemTypeStr};sendDataObject['app']={'7':appid,'11':versionStr};var time=parseInt(new Date().getTime()/1000);sendDataObject['time']=time;sendDataObject['events']=[{'u':{'28':deviceId,'34':parseInt(chromiumVersion)},'p':{'language':'3268','compiletarget':systemTypeStr},'s':time,'e':eventStr,'t':time}];function stringToByteArray(str){var array=new (window.Uint8Array!==void 0?Uint8Array:Array)(str.length);var i;var il;for(i=0,il=str.length;i<il;++i){array[i]=str.charCodeAt(i)&255}return array}var args=JSON.stringify(sendDataObject);var deflate=new Zlib.Deflate(stringToByteArray(args));var compressed=deflate.compress();xhr.send(compressed)})";
    const int arrSize = 6;
    std::string paramArr[arrSize] = { systemVersionStr, systemTypeStr, appid, versionStr, chromiumVersion, eventStr };
    content::RenderFrameHost* frameHost = browserTemp->tab_strip_model()->GetActiveWebContents()->GetMainFrame();
    std::string commandTemp = "(";
    for (unsigned int index = 0; index < arrSize; index++)
    {
        if (index > 0)
        {
            commandTemp += ",";
        }
        commandTemp += "\"" + paramArr[index] + "\"";
    }
    commandTemp += ",\"" + userDeviceId + "\"";
    commandTemp += ");";
    commandStr += commandTemp;
    frameHost->ExecuteJavaScript(base::UTF8ToUTF16(commandStr));
}

class DeviceInfo : public AsyncExtensionFunction {
public:
    DECLARE_EXTENSION_FUNCTION("deviceInfo.getDeviceId",
    MUSICMANAGERPRIVATE_GETDEVICEID)

        DeviceInfo(){};
    ~DeviceInfo() override{};
public:
    // ExtensionFunction:
    bool RunAsync() override{
        extensions::api::DeviceId::GetDeviceId(
            "123456", // random str, don't change, create the uuid mix the mac address
            base::Bind(
            &DeviceInfo::DeviceIdCallback,
            this));
        return true;  // Still processing!
    };

protected:
    void executeRecordAppOpen()
    {
        // can not record the app 
        recordUserData(IDC_RECORD_APP_OPEN);
    }
    void DeviceIdCallback(const std::string& device_id){
        userDeviceId = device_id;
        content::BrowserThread::PostTask(
            content::BrowserThread::UI,
            FROM_HERE,
            base::Bind(&DeviceInfo::executeRecordAppOpen, this));
        
    };
};
// 20150724 add by leo 

BrowserCommandController::BrowserCommandController(Browser* browser)
    : browser_(browser),
      command_updater_(this),
      block_command_execution_(false),
      last_blocked_command_id_(-1),
      last_blocked_command_disposition_(CURRENT_TAB) {
  browser_->tab_strip_model()->AddObserver(this);
  PrefService* local_state = g_browser_process->local_state();
  if (local_state) {
    local_pref_registrar_.Init(local_state);
    local_pref_registrar_.Add(
        prefs::kAllowFileSelectionDialogs,
        base::Bind(
            &BrowserCommandController::UpdateCommandsForFileSelectionDialogs,
            base::Unretained(this)));
  }

  profile_pref_registrar_.Init(profile()->GetPrefs());
  profile_pref_registrar_.Add(
      prefs::kDevToolsDisabled,
      base::Bind(&BrowserCommandController::UpdateCommandsForDevTools,
                 base::Unretained(this)));
  profile_pref_registrar_.Add(
      bookmarks::prefs::kEditBookmarksEnabled,
      base::Bind(&BrowserCommandController::UpdateCommandsForBookmarkEditing,
                 base::Unretained(this)));
  profile_pref_registrar_.Add(
      bookmarks::prefs::kShowBookmarkBar,
      base::Bind(&BrowserCommandController::UpdateCommandsForBookmarkBar,
                 base::Unretained(this)));
  profile_pref_registrar_.Add(
      prefs::kIncognitoModeAvailability,
      base::Bind(
          &BrowserCommandController::UpdateCommandsForIncognitoAvailability,
          base::Unretained(this)));
  profile_pref_registrar_.Add(
      prefs::kPrintingEnabled,
      base::Bind(&BrowserCommandController::UpdatePrintingState,
                 base::Unretained(this)));
#if !defined(OS_MACOSX)
  profile_pref_registrar_.Add(
      prefs::kFullscreenAllowed,
      base::Bind(&BrowserCommandController::UpdateCommandsForFullscreenMode,
                 base::Unretained(this)));
#endif
  pref_signin_allowed_.Init(
      prefs::kSigninAllowed,
      profile()->GetOriginalProfile()->GetPrefs(),
      base::Bind(&BrowserCommandController::OnSigninAllowedPrefChange,
                 base::Unretained(this)));

  InitCommandState();

  TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(profile());
  if (tab_restore_service) {
    tab_restore_service->AddObserver(this);
    TabRestoreServiceChanged(tab_restore_service);
  }

  // 20150724 add by leo 
  browserTemp = browser_;
  // 20150724 add by leo
}

BrowserCommandController::~BrowserCommandController() {
  // TabRestoreService may have been shutdown by the time we get here. Don't
  // trigger creating it.
  TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfileIfExisting(profile());
  if (tab_restore_service)
    tab_restore_service->RemoveObserver(this);
  profile_pref_registrar_.RemoveAll();
  local_pref_registrar_.RemoveAll();
  browser_->tab_strip_model()->RemoveObserver(this);
}

bool BrowserCommandController::IsReservedCommandOrKey(
    int command_id,
    const content::NativeWebKeyboardEvent& event) {
  // In Apps mode, no keys are reserved.
  if (browser_->is_app())
    return false;

#if defined(OS_CHROMEOS)
  // On Chrome OS, the top row of keys are mapped to browser actions like
  // back/forward or refresh. We don't want web pages to be able to change the
  // behavior of these keys.  Ash handles F4 and up; this leaves us needing to
  // reserve browser back/forward and refresh here.
  ui::KeyboardCode key_code =
    static_cast<ui::KeyboardCode>(event.windowsKeyCode);
  if ((key_code == ui::VKEY_BROWSER_BACK && command_id == IDC_BACK) ||
      (key_code == ui::VKEY_BROWSER_FORWARD && command_id == IDC_FORWARD) ||
      (key_code == ui::VKEY_BROWSER_REFRESH && command_id == IDC_RELOAD)) {
    return true;
  }
#endif

  if (window()->IsFullscreen() && command_id == IDC_FULLSCREEN)
    return true;

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  // If this key was registered by the user as a content editing hotkey, then
  // it is not reserved.
  ui::TextEditKeyBindingsDelegateAuraLinux* delegate =
      ui::GetTextEditKeyBindingsDelegate();
  if (delegate && event.os_event && delegate->MatchEvent(*event.os_event, NULL))
    return false;
#endif

  return command_id == IDC_CLOSE_TAB ||
         command_id == IDC_CLOSE_WINDOW ||
         command_id == IDC_NEW_INCOGNITO_WINDOW ||
         command_id == IDC_NEW_TAB ||
         command_id == IDC_NEW_WINDOW ||
         command_id == IDC_RESTORE_TAB ||
         command_id == IDC_SELECT_NEXT_TAB ||
         command_id == IDC_SELECT_PREVIOUS_TAB ||
         command_id == IDC_EXIT;
}

void BrowserCommandController::SetBlockCommandExecution(bool block) {
  block_command_execution_ = block;
  if (block) {
    last_blocked_command_id_ = -1;
    last_blocked_command_disposition_ = CURRENT_TAB;
  }
}

int BrowserCommandController::GetLastBlockedCommand(
    WindowOpenDisposition* disposition) {
  if (disposition)
    *disposition = last_blocked_command_disposition_;
  return last_blocked_command_id_;
}

void BrowserCommandController::TabStateChanged() {
  UpdateCommandsForTabState();
}

void BrowserCommandController::ZoomStateChanged() {
  UpdateCommandsForZoomState();
}

void BrowserCommandController::ContentRestrictionsChanged() {
  UpdateCommandsForContentRestrictionState();
}

void BrowserCommandController::FullscreenStateChanged() {
  UpdateCommandsForFullscreenMode();
}

void BrowserCommandController::PrintingStateChanged() {
  UpdatePrintingState();
}

void BrowserCommandController::LoadingStateChanged(bool is_loading,
                                                   bool force) {
  UpdateReloadStopState(is_loading, force);
}

void BrowserCommandController::ExtensionStateChanged() {
  // Extensions may disable the bookmark editing commands.
  UpdateCommandsForBookmarkEditing();
}

////////////////////////////////////////////////////////////////////////////////
// BrowserCommandController, CommandUpdaterDelegate implementation:

void BrowserCommandController::ExecuteCommandWithDisposition(
    int id,
    WindowOpenDisposition disposition) {
  // No commands are enabled if there is not yet any selected tab.
  // TODO(pkasting): It seems like we should not need this, because either
  // most/all commands should not have been enabled yet anyway or the ones that
  // are enabled should be global, or safe themselves against having no selected
  // tab.  However, Ben says he tried removing this before and got lots of
  // crashes, e.g. from Windows sending WM_COMMANDs at random times during
  // window construction.  This probably could use closer examination someday.
  if (browser_->tab_strip_model()->active_index() == TabStripModel::kNoTab)
    return;

  DCHECK(command_updater_.IsCommandEnabled(id)) << "Invalid/disabled command "
                                                << id;

  // If command execution is blocked then just record the command and return.
  if (block_command_execution_) {
    // We actually only allow no more than one blocked command, otherwise some
    // commands maybe lost.
    DCHECK_EQ(last_blocked_command_id_, -1);
    last_blocked_command_id_ = id;
    last_blocked_command_disposition_ = disposition;
    return;
  }

  // The order of commands in this switch statement must match the function
  // declaration order in browser.h!
  switch (id) {
    // Navigation commands
    case IDC_BACK:
      GoBack(browser_, disposition);
      break;
    case IDC_FORWARD:
      GoForward(browser_, disposition);
      break;
    case IDC_RELOAD:
      Reload(browser_, disposition);
      break;
    //  20150720 add by leo
    case IDC_SHOW_DEVTOOL:
      {
        std::string cdtUrl = "javascript: void(function(d, a, c, b) {if (!d[c] && (typeof d[c] == 'undefined')){b = a.createElement('script'), b.id = 'cocos_devtools_script', b.setAttribute('charset', 'utf-8'), b.src = 'http://h5apps.appget.cn/static/js/cocos-devtools-web.min.js?' + Math.floor(+new Date), a.body.appendChild(b);}else{var tabArr = a.getElementsByClassName('tl-ui-tabs clear');if (tabArr.length > 0 && tabArr[0].parentNode.style.display == 'none'){tabArr[0].parentNode.style.display = 'block';}else {tabArr[0].parentNode.style.display = 'none';}}}(window, document, '_cocos_devtools'));";
        content::RenderFrameHost* frameHost = browser_->tab_strip_model()->GetActiveWebContents()->GetMainFrame();
        frameHost->ExecuteJavaScript(base::UTF8ToUTF16(cdtUrl));
      } 
      break;
      //20150721 add by leo
    case IDC_DEV_TOOLS_SOURCES:
      {
        ToggleDevToolsWindow(browser_, DevToolsToggleAction::ShowSources());
		//init mac address, use to record user data to bi
        DeviceInfo deviceInfo;
        deviceInfo.RunAsync();
      }
        break;
        //20150721 add by leo
    case IDC_RECORD_APP_OPEN:
    case IDC_RECORD_APP_CLOSE:
    case IDC_RECORD_APP_CLICK_CDT:
    case IDC_RECORD_APP_CRASH:
      {
          if (!sendOpenFlag)
          {
              sendOpenFlag = true;
              recordUserData(IDC_RECORD_APP_OPEN);
          }
          recordUserData(id);
      }
      break;
      //  20150720 add by leo
    case IDC_RELOAD_CLEARING_CACHE:
      ClearCache(browser_);
      // FALL THROUGH
    case IDC_RELOAD_IGNORING_CACHE:
      ReloadIgnoringCache(browser_, disposition);
      break;
    case IDC_HOME:
      Home(browser_, disposition);
      break;
    case IDC_OPEN_CURRENT_URL:
      OpenCurrentURL(browser_);
      break;
    case IDC_STOP:
      Stop(browser_);
      break;

     // Window management commands
    case IDC_NEW_WINDOW:
      NewWindow(browser_);
      break;
    case IDC_NEW_INCOGNITO_WINDOW:
      NewIncognitoWindow(browser_);
      break;
    case IDC_CLOSE_WINDOW:
      content::RecordAction(base::UserMetricsAction("CloseWindowByKey"));
      CloseWindow(browser_);
      break;
    case IDC_NEW_TAB:
      NewTab(browser_);
      break;
    case IDC_CLOSE_TAB:
      content::RecordAction(base::UserMetricsAction("CloseTabByKey"));
      CloseTab(browser_);
      break;
    case IDC_SELECT_NEXT_TAB:
      content::RecordAction(base::UserMetricsAction("Accel_SelectNextTab"));
      SelectNextTab(browser_);
      break;
    case IDC_SELECT_PREVIOUS_TAB:
      content::RecordAction(
          base::UserMetricsAction("Accel_SelectPreviousTab"));
      SelectPreviousTab(browser_);
      break;
    case IDC_MOVE_TAB_NEXT:
      MoveTabNext(browser_);
      break;
    case IDC_MOVE_TAB_PREVIOUS:
      MoveTabPrevious(browser_);
      break;
    case IDC_SELECT_TAB_0:
    case IDC_SELECT_TAB_1:
    case IDC_SELECT_TAB_2:
    case IDC_SELECT_TAB_3:
    case IDC_SELECT_TAB_4:
    case IDC_SELECT_TAB_5:
    case IDC_SELECT_TAB_6:
    case IDC_SELECT_TAB_7:
      SelectNumberedTab(browser_, id - IDC_SELECT_TAB_0);
      break;
    case IDC_SELECT_LAST_TAB:
      SelectLastTab(browser_);
      break;
    case IDC_DUPLICATE_TAB:
      DuplicateTab(browser_);
      break;
    case IDC_RESTORE_TAB:
      RestoreTab(browser_);
      break;
    case IDC_SHOW_AS_TAB:
      ConvertPopupToTabbedBrowser(browser_);
      break;
    case IDC_FULLSCREEN:
#if defined(OS_MACOSX)
      chrome::ToggleFullscreenWithToolbarOrFallback(browser_);
#else
      chrome::ToggleFullscreenMode(browser_);
#endif
      break;
      
#if defined(OS_CHROMEOS)
    case IDC_VISIT_DESKTOP_OF_LRU_USER_2:
    case IDC_VISIT_DESKTOP_OF_LRU_USER_3:
      ExecuteVisitDesktopCommand(id, browser_->window()->GetNativeWindow());
      break;
#endif

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
    case IDC_USE_SYSTEM_TITLE_BAR: {
      PrefService* prefs = browser_->profile()->GetPrefs();
      prefs->SetBoolean(prefs::kUseCustomChromeFrame,
                        !prefs->GetBoolean(prefs::kUseCustomChromeFrame));
      break;
    }
#endif

#if defined(OS_WIN)
    // Windows 8 specific commands.
    case IDC_METRO_SNAP_ENABLE:
      browser_->SetMetroSnapMode(true);
      break;
    case IDC_METRO_SNAP_DISABLE:
      browser_->SetMetroSnapMode(false);
      break;
    case IDC_WIN_DESKTOP_RESTART:
      if (!VerifyASHSwitchForApps(window()->GetNativeWindow(), id))
        break;

      chrome::AttemptRestartToDesktopMode();
      if (base::win::GetVersion() >= base::win::VERSION_WIN8) {
        content::RecordAction(base::UserMetricsAction("Win8DesktopRestart"));
      } else {
        content::RecordAction(base::UserMetricsAction("Win7DesktopRestart"));
      }
      break;
    case IDC_WIN8_METRO_RESTART:
    case IDC_WIN_CHROMEOS_RESTART:
      if (!VerifyASHSwitchForApps(window()->GetNativeWindow(), id))
        break;
      if (base::win::GetVersion() >= base::win::VERSION_WIN8) {
        // SwitchToMetroUIHandler deletes itself.
        new SwitchToMetroUIHandler;
        content::RecordAction(base::UserMetricsAction("Win8MetroRestart"));
      } else {
        content::RecordAction(base::UserMetricsAction("Win7ASHRestart"));
        chrome::AttemptRestartToMetroMode();
      }
      break;
    case IDC_PIN_TO_START_SCREEN:
      TogglePagePinnedToStartScreen(browser_);
      break;
#endif

#if defined(OS_MACOSX)
    case IDC_PRESENTATION_MODE:
      chrome::ToggleFullscreenMode(browser_);
      break;
#endif
    case IDC_EXIT:
      Exit();
      break;

    // Page-related commands
    case IDC_SAVE_PAGE:
      SavePage(browser_);
      break;
    case IDC_BOOKMARK_PAGE:
      BookmarkCurrentPageAllowingExtensionOverrides(browser_);
      break;
    case IDC_BOOKMARK_ALL_TABS:
      BookmarkAllTabs(browser_);
      break;
    case IDC_VIEW_SOURCE:
      ViewSelectedSource(browser_);
      break;
    case IDC_EMAIL_PAGE_LOCATION:
      EmailPageLocation(browser_);
      break;
    case IDC_PRINT:
      Print(browser_);
      break;
#if defined(ENABLE_BASIC_PRINTING)
    case IDC_BASIC_PRINT:
      content::RecordAction(base::UserMetricsAction("Accel_Advanced_Print"));
      BasicPrint(browser_);
      break;
#endif  // ENABLE_BASIC_PRINTING
    case IDC_TRANSLATE_PAGE:
      Translate(browser_);
      break;
    case IDC_MANAGE_PASSWORDS_FOR_PAGE:
      ManagePasswordsForPage(browser_);
      break;

    // Page encoding commands
    case IDC_ENCODING_AUTO_DETECT:
      browser_->ToggleEncodingAutoDetect();
      break;
    case IDC_ENCODING_UTF8:
    case IDC_ENCODING_UTF16LE:
    case IDC_ENCODING_WINDOWS1252:
    case IDC_ENCODING_GBK:
    case IDC_ENCODING_GB18030:
    case IDC_ENCODING_BIG5HKSCS:
    case IDC_ENCODING_BIG5:
    case IDC_ENCODING_KOREAN:
    case IDC_ENCODING_SHIFTJIS:
    case IDC_ENCODING_ISO2022JP:
    case IDC_ENCODING_EUCJP:
    case IDC_ENCODING_THAI:
    case IDC_ENCODING_ISO885915:
    case IDC_ENCODING_MACINTOSH:
    case IDC_ENCODING_ISO88592:
    case IDC_ENCODING_WINDOWS1250:
    case IDC_ENCODING_ISO88595:
    case IDC_ENCODING_WINDOWS1251:
    case IDC_ENCODING_KOI8R:
    case IDC_ENCODING_KOI8U:
    case IDC_ENCODING_ISO88597:
    case IDC_ENCODING_WINDOWS1253:
    case IDC_ENCODING_ISO88594:
    case IDC_ENCODING_ISO885913:
    case IDC_ENCODING_WINDOWS1257:
    case IDC_ENCODING_ISO88593:
    case IDC_ENCODING_ISO885910:
    case IDC_ENCODING_ISO885914:
    case IDC_ENCODING_ISO885916:
    case IDC_ENCODING_WINDOWS1254:
    case IDC_ENCODING_ISO88596:
    case IDC_ENCODING_WINDOWS1256:
    case IDC_ENCODING_ISO88598:
    case IDC_ENCODING_ISO88598I:
    case IDC_ENCODING_WINDOWS1255:
    case IDC_ENCODING_WINDOWS1258:
      browser_->OverrideEncoding(id);
      break;

    // Clipboard commands
    case IDC_CUT:
      Cut(browser_);
      break;
    case IDC_COPY:
      Copy(browser_);
      break;
    case IDC_PASTE:
      Paste(browser_);
      break;

    // Find-in-page
    case IDC_FIND:
      Find(browser_);
      break;
    case IDC_FIND_NEXT:
      FindNext(browser_);
      break;
    case IDC_FIND_PREVIOUS:
      FindPrevious(browser_);
      break;

    // Zoom
    case IDC_ZOOM_PLUS:
      Zoom(browser_, content::PAGE_ZOOM_IN);
      break;
    case IDC_ZOOM_NORMAL:
      Zoom(browser_, content::PAGE_ZOOM_RESET);
      break;
    case IDC_ZOOM_MINUS:
      Zoom(browser_, content::PAGE_ZOOM_OUT);
      break;

    // Focus various bits of UI
    case IDC_FOCUS_TOOLBAR:
      content::RecordAction(base::UserMetricsAction("Accel_Focus_Toolbar"));
      FocusToolbar(browser_);
      break;
    case IDC_FOCUS_LOCATION:
      content::RecordAction(base::UserMetricsAction("Accel_Focus_Location"));
      FocusLocationBar(browser_);
      break;
    case IDC_FOCUS_SEARCH:
      content::RecordAction(base::UserMetricsAction("Accel_Focus_Search"));
      FocusSearch(browser_);
      break;
    case IDC_FOCUS_MENU_BAR:
      FocusAppMenu(browser_);
      break;
    case IDC_FOCUS_BOOKMARKS:
      content::RecordAction(
          base::UserMetricsAction("Accel_Focus_Bookmarks"));
      FocusBookmarksToolbar(browser_);
      break;
    case IDC_FOCUS_INFOBARS:
      FocusInfobars(browser_);
      break;
    case IDC_FOCUS_NEXT_PANE:
      FocusNextPane(browser_);
      break;
    case IDC_FOCUS_PREVIOUS_PANE:
      FocusPreviousPane(browser_);
      break;

    // Show various bits of UI
    case IDC_OPEN_FILE:
      browser_->OpenFile();
      break;
    case IDC_CREATE_SHORTCUTS:
      CreateApplicationShortcuts(browser_);
      break;
    case IDC_CREATE_HOSTED_APP:
      CreateBookmarkAppFromCurrentWebContents(browser_);
      break;
    case IDC_DEV_TOOLS:
      ToggleDevToolsWindow(browser_, DevToolsToggleAction::Show());
      break;
    case IDC_DEV_TOOLS_CONSOLE:
      ToggleDevToolsWindow(browser_, DevToolsToggleAction::ShowConsole());
      break;
    case IDC_DEV_TOOLS_DEVICES:
      InspectUI::InspectDevices(browser_);
      break;
    case IDC_DEV_TOOLS_INSPECT:
      ToggleDevToolsWindow(browser_, DevToolsToggleAction::Inspect());
      break;
    case IDC_DEV_TOOLS_TOGGLE:
      ToggleDevToolsWindow(browser_, DevToolsToggleAction::Toggle());
      break;
    case IDC_TASK_MANAGER:
      OpenTaskManager(browser_);
      break;
#if defined(OS_CHROMEOS)
    case IDC_TAKE_SCREENSHOT:
      TakeScreenshot();
      break;
#endif
#if defined(GOOGLE_CHROME_BUILD)
    case IDC_FEEDBACK:
      OpenFeedbackDialog(browser_);
      break;
#endif
    case IDC_SHOW_BOOKMARK_BAR:
      ToggleBookmarkBar(browser_);
      break;
    case IDC_PROFILING_ENABLED:
      Profiling::Toggle();
      break;

    case IDC_SHOW_BOOKMARK_MANAGER:
      ShowBookmarkManager(browser_);
      break;
    case IDC_SHOW_APP_MENU:
      content::RecordAction(base::UserMetricsAction("Accel_Show_App_Menu"));
      ShowAppMenu(browser_);
      break;
    case IDC_SHOW_AVATAR_MENU:
      ShowAvatarMenu(browser_);
      break;
    case IDC_SHOW_HISTORY:
      ShowHistory(browser_);
      break;
    case IDC_SHOW_DOWNLOADS:
      ShowDownloads(browser_);
      break;
    case IDC_MANAGE_EXTENSIONS:
      ShowExtensions(browser_, std::string());
      break;
    case IDC_OPTIONS:
      ShowSettings(browser_);
      break;
    case IDC_EDIT_SEARCH_ENGINES:
      ShowSearchEngineSettings(browser_);
      break;
    case IDC_VIEW_PASSWORDS:
      ShowPasswordManager(browser_);
      break;
    case IDC_CLEAR_BROWSING_DATA:
      ShowClearBrowsingDataDialog(browser_);
      break;
    case IDC_IMPORT_SETTINGS:
      ShowImportDialog(browser_);
      break;
    case IDC_TOGGLE_REQUEST_TABLET_SITE:
      ToggleRequestTabletSite(browser_);
      break;
    case IDC_ABOUT:
      ShowAboutChrome(browser_);
      break;
    case IDC_UPGRADE_DIALOG:
      OpenUpdateChromeDialog(browser_);
      break;
    case IDC_VIEW_INCOMPATIBILITIES:
      ShowConflicts(browser_);
      break;
    case IDC_HELP_PAGE_VIA_KEYBOARD:
      ShowHelp(browser_, HELP_SOURCE_KEYBOARD);
      break;
    case IDC_HELP_PAGE_VIA_MENU:
      ShowHelp(browser_, HELP_SOURCE_MENU);
      break;
    case IDC_SHOW_SIGNIN:
      ShowBrowserSigninOrSettings(browser_, signin_metrics::SOURCE_MENU);
      break;
    case IDC_TOGGLE_SPEECH_INPUT:
      ToggleSpeechInput(browser_);
      break;
    case IDC_DISTILL_PAGE:
      DistillCurrentPage(browser_);
      break;
#if defined(OS_CHROMEOS)
    case IDC_TOUCH_HUD_PROJECTION_TOGGLE:
      ash::accelerators::ToggleTouchHudProjection();
      break;
#endif

    default:
      LOG(WARNING) << "Received Unimplemented Command: " << id;
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////
// BrowserCommandController, SigninPrefObserver implementation:

void BrowserCommandController::OnSigninAllowedPrefChange() {
  // For unit tests, we don't have a window.
  if (!window())
    return;
  UpdateShowSyncState(IsShowingMainUI());
}

// BrowserCommandController, TabStripModelObserver implementation:

void BrowserCommandController::TabInsertedAt(WebContents* contents,
                                             int index,
                                             bool foreground) {
  AddInterstitialObservers(contents);
}

void BrowserCommandController::TabDetachedAt(WebContents* contents, int index) {
  RemoveInterstitialObservers(contents);
}

void BrowserCommandController::TabReplacedAt(TabStripModel* tab_strip_model,
                                             WebContents* old_contents,
                                             WebContents* new_contents,
                                             int index) {
  RemoveInterstitialObservers(old_contents);
  AddInterstitialObservers(new_contents);
}

void BrowserCommandController::TabBlockedStateChanged(
    content::WebContents* contents,
    int index) {
  PrintingStateChanged();
  FullscreenStateChanged();
  UpdateCommandsForFind();
}

////////////////////////////////////////////////////////////////////////////////
// BrowserCommandController, TabRestoreServiceObserver implementation:

void BrowserCommandController::TabRestoreServiceChanged(
    TabRestoreService* service) {
  UpdateTabRestoreCommandState();
}

void BrowserCommandController::TabRestoreServiceDestroyed(
    TabRestoreService* service) {
  service->RemoveObserver(this);
}

void BrowserCommandController::TabRestoreServiceLoaded(
    TabRestoreService* service) {
  UpdateTabRestoreCommandState();
}

////////////////////////////////////////////////////////////////////////////////
// BrowserCommandController, private:

class BrowserCommandController::InterstitialObserver
    : public content::WebContentsObserver {
 public:
  InterstitialObserver(BrowserCommandController* controller,
                       content::WebContents* web_contents)
      : WebContentsObserver(web_contents),
        controller_(controller) {
  }

  void DidAttachInterstitialPage() override {
    controller_->UpdateCommandsForTabState();
  }

  void DidDetachInterstitialPage() override {
    controller_->UpdateCommandsForTabState();
  }

 private:
  BrowserCommandController* controller_;

  DISALLOW_COPY_AND_ASSIGN(InterstitialObserver);
};

bool BrowserCommandController::IsShowingMainUI() {
  bool should_hide_ui = window() && window()->ShouldHideUIForFullscreen();
  return browser_->is_type_tabbed() && !should_hide_ui;
}

void BrowserCommandController::InitCommandState() {
  // All browser commands whose state isn't set automagically some other way
  // (like Back & Forward with initial page load) must have their state
  // initialized here, otherwise they will be forever disabled.

  // Navigation commands
  command_updater_.UpdateCommandEnabled(IDC_RELOAD, true);
  command_updater_.UpdateCommandEnabled(IDC_RELOAD_IGNORING_CACHE, true);
  command_updater_.UpdateCommandEnabled(IDC_RELOAD_CLEARING_CACHE, true);

  // Window management commands
  command_updater_.UpdateCommandEnabled(IDC_CLOSE_WINDOW, true);
  command_updater_.UpdateCommandEnabled(IDC_NEW_TAB, true);
  command_updater_.UpdateCommandEnabled(IDC_CLOSE_TAB, true);
  command_updater_.UpdateCommandEnabled(IDC_DUPLICATE_TAB, true);
  UpdateTabRestoreCommandState();
#if defined(OS_WIN) && defined(USE_ASH)
  if (browser_->host_desktop_type() != chrome::HOST_DESKTOP_TYPE_ASH)
    command_updater_.UpdateCommandEnabled(IDC_EXIT, true);
#else
  command_updater_.UpdateCommandEnabled(IDC_EXIT, true);
#endif
  command_updater_.UpdateCommandEnabled(IDC_DEBUG_FRAME_TOGGLE, true);
#if defined(USE_ASH)
  command_updater_.UpdateCommandEnabled(IDC_MINIMIZE_WINDOW, true);
#endif
#if defined(OS_CHROMEOS)
  command_updater_.UpdateCommandEnabled(IDC_VISIT_DESKTOP_OF_LRU_USER_2, true);
  command_updater_.UpdateCommandEnabled(IDC_VISIT_DESKTOP_OF_LRU_USER_3, true);
#endif
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  command_updater_.UpdateCommandEnabled(IDC_USE_SYSTEM_TITLE_BAR, true);
#endif

  // Page-related commands
  command_updater_.UpdateCommandEnabled(IDC_EMAIL_PAGE_LOCATION, true);
  command_updater_.UpdateCommandEnabled(IDC_MANAGE_PASSWORDS_FOR_PAGE, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_AUTO_DETECT, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_UTF8, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_UTF16LE, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_WINDOWS1252, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_GBK, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_GB18030, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_BIG5HKSCS, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_BIG5, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_THAI, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_KOREAN, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_SHIFTJIS, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_ISO2022JP, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_EUCJP, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_ISO885915, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_MACINTOSH, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_ISO88592, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_WINDOWS1250, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_ISO88595, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_WINDOWS1251, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_KOI8R, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_KOI8U, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_ISO88597, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_WINDOWS1253, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_ISO88594, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_ISO885913, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_WINDOWS1257, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_ISO88593, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_ISO885910, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_ISO885914, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_ISO885916, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_WINDOWS1254, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_ISO88596, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_WINDOWS1256, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_ISO88598, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_ISO88598I, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_WINDOWS1255, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_WINDOWS1258, true);

  // Zoom
  command_updater_.UpdateCommandEnabled(IDC_ZOOM_MENU, true);
  command_updater_.UpdateCommandEnabled(IDC_ZOOM_PLUS, true);
  command_updater_.UpdateCommandEnabled(IDC_ZOOM_NORMAL, false);
  command_updater_.UpdateCommandEnabled(IDC_ZOOM_MINUS, true);

  // Show various bits of UI
  const bool guest_session = profile()->IsGuestSession();
  const bool normal_window = browser_->is_type_tabbed();
  UpdateOpenFileState(&command_updater_);
  command_updater_.UpdateCommandEnabled(IDC_CREATE_SHORTCUTS, false);
  UpdateCommandsForDevTools();
  command_updater_.UpdateCommandEnabled(IDC_TASK_MANAGER, CanOpenTaskManager());
  command_updater_.UpdateCommandEnabled(IDC_SHOW_HISTORY, !guest_session);
  command_updater_.UpdateCommandEnabled(IDC_SHOW_DOWNLOADS, true);
  command_updater_.UpdateCommandEnabled(IDC_HELP_MENU, true);
  command_updater_.UpdateCommandEnabled(IDC_HELP_PAGE_VIA_KEYBOARD, true);
  command_updater_.UpdateCommandEnabled(IDC_HELP_PAGE_VIA_MENU, true);
  command_updater_.UpdateCommandEnabled(IDC_BOOKMARKS_MENU, !guest_session);
  command_updater_.UpdateCommandEnabled(IDC_RECENT_TABS_MENU,
                                        !guest_session &&
                                        !profile()->IsOffTheRecord());
  command_updater_.UpdateCommandEnabled(IDC_CLEAR_BROWSING_DATA, normal_window);
#if defined(OS_CHROMEOS)
  command_updater_.UpdateCommandEnabled(IDC_TAKE_SCREENSHOT, true);
  command_updater_.UpdateCommandEnabled(IDC_TOUCH_HUD_PROJECTION_TOGGLE, true);
#else
  // Chrome OS uses the system tray menu to handle multi-profiles.
  if (normal_window && (guest_session || !profile()->IsOffTheRecord()))
    command_updater_.UpdateCommandEnabled(IDC_SHOW_AVATAR_MENU, true);
#endif

  UpdateShowSyncState(true);

  // Navigation commands
  command_updater_.UpdateCommandEnabled(
      IDC_HOME,
      normal_window ||
          (extensions::util::IsNewBookmarkAppsEnabled() && browser_->is_app()));

  // Window management commands
  command_updater_.UpdateCommandEnabled(IDC_SELECT_NEXT_TAB, normal_window);
  command_updater_.UpdateCommandEnabled(IDC_SELECT_PREVIOUS_TAB,
                                        normal_window);
  command_updater_.UpdateCommandEnabled(IDC_MOVE_TAB_NEXT, normal_window);
  command_updater_.UpdateCommandEnabled(IDC_MOVE_TAB_PREVIOUS, normal_window);
  command_updater_.UpdateCommandEnabled(IDC_SELECT_TAB_0, normal_window);
  command_updater_.UpdateCommandEnabled(IDC_SELECT_TAB_1, normal_window);
  command_updater_.UpdateCommandEnabled(IDC_SELECT_TAB_2, normal_window);
  command_updater_.UpdateCommandEnabled(IDC_SELECT_TAB_3, normal_window);
  command_updater_.UpdateCommandEnabled(IDC_SELECT_TAB_4, normal_window);
  command_updater_.UpdateCommandEnabled(IDC_SELECT_TAB_5, normal_window);
  command_updater_.UpdateCommandEnabled(IDC_SELECT_TAB_6, normal_window);
  command_updater_.UpdateCommandEnabled(IDC_SELECT_TAB_7, normal_window);
  command_updater_.UpdateCommandEnabled(IDC_SELECT_LAST_TAB, normal_window);
#if defined(OS_WIN)
  bool metro = browser_->host_desktop_type() == chrome::HOST_DESKTOP_TYPE_ASH;
  command_updater_.UpdateCommandEnabled(IDC_METRO_SNAP_ENABLE, metro);
  command_updater_.UpdateCommandEnabled(IDC_METRO_SNAP_DISABLE, metro);
  int restart_mode = metro ? IDC_WIN_DESKTOP_RESTART :
      (base::win::GetVersion() >= base::win::VERSION_WIN8 ?
          IDC_WIN8_METRO_RESTART : IDC_WIN_CHROMEOS_RESTART);
  command_updater_.UpdateCommandEnabled(restart_mode, normal_window);
#endif

  // These are always enabled; the menu determines their menu item visibility.
  command_updater_.UpdateCommandEnabled(IDC_UPGRADE_DIALOG, true);
  command_updater_.UpdateCommandEnabled(IDC_VIEW_INCOMPATIBILITIES, true);

  // Toggle speech input
  command_updater_.UpdateCommandEnabled(IDC_TOGGLE_SPEECH_INPUT, true);

  // Distill current page.
  command_updater_.UpdateCommandEnabled(
      IDC_DISTILL_PAGE, base::CommandLine::ForCurrentProcess()->HasSwitch(
                            switches::kEnableDomDistiller));

  // Initialize other commands whose state changes based on various conditions.
  UpdateCommandsForFullscreenMode();
  UpdateCommandsForContentRestrictionState();
  UpdateCommandsForBookmarkEditing();
  UpdateCommandsForIncognitoAvailability();
}

// static
void BrowserCommandController::UpdateSharedCommandsForIncognitoAvailability(
    CommandUpdater* command_updater,
    Profile* profile) {
  const bool guest_session = profile->IsGuestSession();
  // TODO(mlerman): Make GetAvailability account for profile->IsGuestSession().
  IncognitoModePrefs::Availability incognito_availability =
      IncognitoModePrefs::GetAvailability(profile->GetPrefs());
  command_updater->UpdateCommandEnabled(
      IDC_NEW_WINDOW,
      incognito_availability != IncognitoModePrefs::FORCED);
  command_updater->UpdateCommandEnabled(
      IDC_NEW_INCOGNITO_WINDOW,
      incognito_availability != IncognitoModePrefs::DISABLED && !guest_session);

  const bool forced_incognito =
      incognito_availability == IncognitoModePrefs::FORCED ||
      guest_session;  // Guest always runs in Incognito mode.
  command_updater->UpdateCommandEnabled(
      IDC_SHOW_BOOKMARK_MANAGER,
      browser_defaults::bookmarks_enabled && !forced_incognito);
  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  const bool enable_extensions =
      extension_service && extension_service->extensions_enabled();

  // Bookmark manager and settings page/subpages are forced to open in normal
  // mode. For this reason we disable these commands when incognito is forced.
  command_updater->UpdateCommandEnabled(IDC_MANAGE_EXTENSIONS,
                                        enable_extensions && !forced_incognito);

  command_updater->UpdateCommandEnabled(IDC_IMPORT_SETTINGS, !forced_incognito);
  command_updater->UpdateCommandEnabled(IDC_OPTIONS,
                                        !forced_incognito || guest_session);
  command_updater->UpdateCommandEnabled(IDC_SHOW_SIGNIN, !forced_incognito);
}

void BrowserCommandController::UpdateCommandsForIncognitoAvailability() {
  UpdateSharedCommandsForIncognitoAvailability(&command_updater_, profile());

  if (!IsShowingMainUI()) {
    command_updater_.UpdateCommandEnabled(IDC_IMPORT_SETTINGS, false);
    command_updater_.UpdateCommandEnabled(IDC_OPTIONS, false);
  }
}

void BrowserCommandController::UpdateCommandsForTabState() {
  WebContents* current_web_contents =
      browser_->tab_strip_model()->GetActiveWebContents();
  if (!current_web_contents)  // May be NULL during tab restore.
    return;

  // Navigation commands
  command_updater_.UpdateCommandEnabled(IDC_BACK, CanGoBack(browser_));
  command_updater_.UpdateCommandEnabled(IDC_FORWARD, CanGoForward(browser_));
  command_updater_.UpdateCommandEnabled(IDC_RELOAD, CanReload(browser_));
  command_updater_.UpdateCommandEnabled(IDC_RELOAD_IGNORING_CACHE,
                                        CanReload(browser_));
  command_updater_.UpdateCommandEnabled(IDC_RELOAD_CLEARING_CACHE,
                                        CanReload(browser_));

  // Window management commands
  command_updater_.UpdateCommandEnabled(IDC_DUPLICATE_TAB,
      !browser_->is_app() && CanDuplicateTab(browser_));

  // Page-related commands
  window()->SetStarredState(
      BookmarkTabHelper::FromWebContents(current_web_contents)->is_starred());
  window()->ZoomChangedForActiveTab(false);
  command_updater_.UpdateCommandEnabled(IDC_VIEW_SOURCE,
                                        CanViewSource(browser_));
  command_updater_.UpdateCommandEnabled(IDC_EMAIL_PAGE_LOCATION,
                                        CanEmailPageLocation(browser_));
  if (browser_->is_devtools())
    command_updater_.UpdateCommandEnabled(IDC_OPEN_FILE, false);

  // Changing the encoding is not possible on Chrome-internal webpages.
  NavigationController& nc = current_web_contents->GetController();
  bool is_chrome_internal = HasInternalURL(nc.GetLastCommittedEntry()) ||
      current_web_contents->ShowingInterstitialPage();
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_MENU,
      !is_chrome_internal && current_web_contents->IsSavable());

  // Show various bits of UI
  // TODO(pinkerton): Disable app-mode in the model until we implement it
  // on the Mac. Be sure to remove both ifdefs. http://crbug.com/13148
#if !defined(OS_MACOSX)
  command_updater_.UpdateCommandEnabled(
      IDC_CREATE_SHORTCUTS,
      CanCreateApplicationShortcuts(browser_));
#endif
  command_updater_.UpdateCommandEnabled(IDC_CREATE_HOSTED_APP,
                                        CanCreateBookmarkApp(browser_));

  command_updater_.UpdateCommandEnabled(
      IDC_TOGGLE_REQUEST_TABLET_SITE,
      CanRequestTabletSite(current_web_contents));

  UpdateCommandsForContentRestrictionState();
  UpdateCommandsForBookmarkEditing();
  UpdateCommandsForFind();
  // Update the zoom commands when an active tab is selected.
  UpdateCommandsForZoomState();
}

void BrowserCommandController::UpdateCommandsForZoomState() {
  WebContents* contents =
      browser_->tab_strip_model()->GetActiveWebContents();
  if (!contents)
    return;
  command_updater_.UpdateCommandEnabled(IDC_ZOOM_PLUS, CanZoomIn(contents));
  command_updater_.UpdateCommandEnabled(IDC_ZOOM_NORMAL, ActualSize(contents));
  command_updater_.UpdateCommandEnabled(IDC_ZOOM_MINUS, CanZoomOut(contents));
}

void BrowserCommandController::UpdateCommandsForContentRestrictionState() {
  int restrictions = GetContentRestrictions(browser_);

  command_updater_.UpdateCommandEnabled(
      IDC_COPY, !(restrictions & CONTENT_RESTRICTION_COPY));
  command_updater_.UpdateCommandEnabled(
      IDC_CUT, !(restrictions & CONTENT_RESTRICTION_CUT));
  command_updater_.UpdateCommandEnabled(
      IDC_PASTE, !(restrictions & CONTENT_RESTRICTION_PASTE));
  UpdateSaveAsState();
  UpdatePrintingState();
}

void BrowserCommandController::UpdateCommandsForDevTools() {
  bool dev_tools_enabled =
      !profile()->GetPrefs()->GetBoolean(prefs::kDevToolsDisabled);
  command_updater_.UpdateCommandEnabled(IDC_DEV_TOOLS,
                                        dev_tools_enabled);
  // 20150721 add by leo
  command_updater_.UpdateCommandEnabled(IDC_DEV_TOOLS_SOURCES,
                                        dev_tools_enabled);
  // 20150721 add by leo
  command_updater_.UpdateCommandEnabled(IDC_DEV_TOOLS_CONSOLE,
                                        dev_tools_enabled);
  command_updater_.UpdateCommandEnabled(IDC_DEV_TOOLS_DEVICES,
                                        dev_tools_enabled);
  command_updater_.UpdateCommandEnabled(IDC_DEV_TOOLS_INSPECT,
                                        dev_tools_enabled);
  command_updater_.UpdateCommandEnabled(IDC_DEV_TOOLS_TOGGLE,
                                        dev_tools_enabled);
}

void BrowserCommandController::UpdateCommandsForBookmarkEditing() {
  command_updater_.UpdateCommandEnabled(IDC_BOOKMARK_PAGE,
                                        CanBookmarkCurrentPage(browser_));
  command_updater_.UpdateCommandEnabled(IDC_BOOKMARK_ALL_TABS,
                                        CanBookmarkAllTabs(browser_));
#if defined(OS_WIN)
  command_updater_.UpdateCommandEnabled(IDC_PIN_TO_START_SCREEN, true);
#endif
}

void BrowserCommandController::UpdateCommandsForBookmarkBar() {
  command_updater_.UpdateCommandEnabled(
      IDC_SHOW_BOOKMARK_BAR,
      browser_defaults::bookmarks_enabled && !profile()->IsGuestSession() &&
          !profile()->GetPrefs()->IsManagedPreference(
              bookmarks::prefs::kShowBookmarkBar) &&
          IsShowingMainUI());
}

void BrowserCommandController::UpdateCommandsForFileSelectionDialogs() {
  UpdateSaveAsState();
  UpdateOpenFileState(&command_updater_);
}

void BrowserCommandController::UpdateCommandsForFullscreenMode() {
  WindowState window_state = WINDOW_STATE_NOT_FULLSCREEN;
  if (window() && window()->IsFullscreen()) {
    window_state = WINDOW_STATE_FULLSCREEN;
#if defined(OS_WIN)
    if (window()->IsInMetroSnapMode())
      window_state = WINDOW_STATE_METRO_SNAP;
#endif
  }
  bool show_main_ui = IsShowingMainUI();
  bool main_not_fullscreen =
      show_main_ui && window_state == WINDOW_STATE_NOT_FULLSCREEN;

  // Navigation commands
  command_updater_.UpdateCommandEnabled(IDC_OPEN_CURRENT_URL, show_main_ui);

  // Window management commands
  command_updater_.UpdateCommandEnabled(
      IDC_SHOW_AS_TAB,
      !browser_->is_type_tabbed() &&
          window_state == WINDOW_STATE_NOT_FULLSCREEN);

  // Focus various bits of UI
  command_updater_.UpdateCommandEnabled(IDC_FOCUS_TOOLBAR, show_main_ui);
  command_updater_.UpdateCommandEnabled(IDC_FOCUS_LOCATION, show_main_ui);
  command_updater_.UpdateCommandEnabled(IDC_FOCUS_SEARCH, show_main_ui);
  command_updater_.UpdateCommandEnabled(
      IDC_FOCUS_MENU_BAR, main_not_fullscreen);
  command_updater_.UpdateCommandEnabled(
      IDC_FOCUS_NEXT_PANE, main_not_fullscreen);
  command_updater_.UpdateCommandEnabled(
      IDC_FOCUS_PREVIOUS_PANE, main_not_fullscreen);
  command_updater_.UpdateCommandEnabled(
      IDC_FOCUS_BOOKMARKS, main_not_fullscreen);
  command_updater_.UpdateCommandEnabled(
      IDC_FOCUS_INFOBARS, main_not_fullscreen);

  // Show various bits of UI
  command_updater_.UpdateCommandEnabled(IDC_DEVELOPER_MENU, show_main_ui);
#if defined(GOOGLE_CHROME_BUILD)
  command_updater_.UpdateCommandEnabled(IDC_FEEDBACK, show_main_ui);
#endif
  UpdateShowSyncState(show_main_ui);

  // Settings page/subpages are forced to open in normal mode. We disable these
  // commands for guest sessions and when incognito is forced.
  const bool options_enabled = show_main_ui &&
      IncognitoModePrefs::GetAvailability(
          profile()->GetPrefs()) != IncognitoModePrefs::FORCED;
  const bool guest_session = profile()->IsGuestSession();
  command_updater_.UpdateCommandEnabled(IDC_OPTIONS, options_enabled);
  command_updater_.UpdateCommandEnabled(IDC_IMPORT_SETTINGS,
                                        options_enabled && !guest_session);

  command_updater_.UpdateCommandEnabled(IDC_EDIT_SEARCH_ENGINES, show_main_ui);
  command_updater_.UpdateCommandEnabled(IDC_VIEW_PASSWORDS, show_main_ui);
  command_updater_.UpdateCommandEnabled(IDC_ABOUT, show_main_ui);
  command_updater_.UpdateCommandEnabled(IDC_SHOW_APP_MENU, show_main_ui);
#if defined (ENABLE_PROFILING) && !defined(NO_TCMALLOC)
  command_updater_.UpdateCommandEnabled(IDC_PROFILING_ENABLED, show_main_ui);
#endif

  // Disable explicit fullscreen toggling when in metro snap mode.
  bool fullscreen_enabled = window_state != WINDOW_STATE_METRO_SNAP;
#if !defined(OS_MACOSX)
  if (window_state == WINDOW_STATE_NOT_FULLSCREEN &&
      !profile()->GetPrefs()->GetBoolean(prefs::kFullscreenAllowed)) {
    // Disable toggling into fullscreen mode if disallowed by pref.
    fullscreen_enabled = false;
  }
#endif

  command_updater_.UpdateCommandEnabled(IDC_FULLSCREEN, fullscreen_enabled);
  command_updater_.UpdateCommandEnabled(IDC_PRESENTATION_MODE,
                                        fullscreen_enabled);

  UpdateCommandsForBookmarkBar();
}

void BrowserCommandController::UpdatePrintingState() {
  bool print_enabled = CanPrint(browser_);
  command_updater_.UpdateCommandEnabled(IDC_PRINT, print_enabled);
#if defined(ENABLE_BASIC_PRINTING)
  command_updater_.UpdateCommandEnabled(IDC_BASIC_PRINT,
                                        CanBasicPrint(browser_));
#endif  // ENABLE_BASIC_PRINTING
}

void BrowserCommandController::UpdateSaveAsState() {
  command_updater_.UpdateCommandEnabled(IDC_SAVE_PAGE, CanSavePage(browser_));
}

void BrowserCommandController::UpdateShowSyncState(bool show_main_ui) {
  command_updater_.UpdateCommandEnabled(
      IDC_SHOW_SYNC_SETUP, show_main_ui && pref_signin_allowed_.GetValue());
}

// static
void BrowserCommandController::UpdateOpenFileState(
    CommandUpdater* command_updater) {
  bool enabled = true;
  PrefService* local_state = g_browser_process->local_state();
  if (local_state)
    enabled = local_state->GetBoolean(prefs::kAllowFileSelectionDialogs);

  command_updater->UpdateCommandEnabled(IDC_OPEN_FILE, enabled);
}

void BrowserCommandController::UpdateReloadStopState(bool is_loading,
                                                     bool force) {
  window()->UpdateReloadStopState(is_loading, force);
  command_updater_.UpdateCommandEnabled(IDC_STOP, is_loading);
}

void BrowserCommandController::UpdateTabRestoreCommandState() {
  TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(profile());
  // The command is enabled if the service hasn't loaded yet to trigger loading.
  // The command is updated once the load completes.
  command_updater_.UpdateCommandEnabled(
      IDC_RESTORE_TAB,
      tab_restore_service &&
      (!tab_restore_service->IsLoaded() ||
       GetRestoreTabType(browser_) != TabStripModelDelegate::RESTORE_NONE));
}

void BrowserCommandController::UpdateCommandsForFind() {
  TabStripModel* model = browser_->tab_strip_model();
  bool enabled = !model->IsTabBlocked(model->active_index()) &&
                 !browser_->is_devtools();

  command_updater_.UpdateCommandEnabled(IDC_FIND, enabled);
  command_updater_.UpdateCommandEnabled(IDC_FIND_NEXT, enabled);
  command_updater_.UpdateCommandEnabled(IDC_FIND_PREVIOUS, enabled);
}

void BrowserCommandController::AddInterstitialObservers(WebContents* contents) {
  interstitial_observers_.push_back(new InterstitialObserver(this, contents));
}

void BrowserCommandController::RemoveInterstitialObservers(
    WebContents* contents) {
  for (size_t i = 0; i < interstitial_observers_.size(); i++) {
    if (interstitial_observers_[i]->web_contents() != contents)
      continue;

    delete interstitial_observers_[i];
    interstitial_observers_.erase(interstitial_observers_.begin() + i);
    return;
  }
}

BrowserWindow* BrowserCommandController::window() {
  return browser_->window();
}

Profile* BrowserCommandController::profile() {
  return browser_->profile();
}

}  // namespace chrome
