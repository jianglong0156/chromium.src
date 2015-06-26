#include "chrome/browser/ui/webui/hello_world_ui.h"
 
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

HelloWorldUI::HelloWorldUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  // Set up the chrome://hello-world source.
  content::WebUIDataSource* html_source =
	  content::WebUIDataSource::Create(chrome::kChromeUICocosUpdateVersionHost);
 
  // Register callback handler.
  this->web_ui()->RegisterMessageCallback("showUpdateDialog",
	  base::Bind(&HelloWorldUI::showUpdateDialog,
  base::Unretained(this)));
 
  //html_source->SetUseJsonJSFormatV2();
 
  // Localized strings.
  html_source->AddLocalizedString("helloWorldTitle", IDS_HELLO_WORLD_TITLE);
  html_source->AddLocalizedString("welcomeMessage", IDS_HELLO_WORLD_WELCOME_TEXT);
 
  // As a demonstration of passing a variable for JS to use we pass in the name "Bob".
  html_source->AddString("userName", "Bob");
  html_source->SetJsonPath("strings.js");
 
  // Add required resources.
  html_source->AddResourcePath("hello_world.css", IDR_HELLO_WORLD_CSS);
  html_source->AddResourcePath("hello_world.js", IDR_HELLO_WORLD_JS);
  html_source->SetDefaultResource(IDR_HELLO_WORLD_HTML);
 
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, html_source);
}
 
HelloWorldUI::~HelloWorldUI() {
}
 
void HelloWorldUI::showUpdateDialog(const base::ListValue* args) {
//   int term1, term2;
//   if (!args->GetInteger(0, &term1) || !args->GetInteger(1, &term2))
//     return;
//   base::FundamentalValue result(term1 + term2);
//   web_ui()->CallJavascriptFunction("hello_world.addResult", result);

	base::StringValue titleStr(l10n_util::GetStringUTF16(IDS_COCOS_UPDATE_VERSION_TITLE));
	base::StringValue contentStr(l10n_util::GetStringUTF16(IDS_COCOS_UPDATE_VERSION_TEXT));
	base::StringValue yesStr(l10n_util::GetStringUTF16(IDS_CONFIRM_MESSAGEBOX_YES_BUTTON_LABEL));
	base::StringValue noStr(l10n_util::GetStringUTF16(IDS_CONFIRM_MESSAGEBOX_NO_BUTTON_LABEL));
	//base::FundamentalValue result(term1 + term2);

	web_ui()->CallJavascriptFunction("hello_world.addResult", titleStr, contentStr, yesStr, noStr);
}
