/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "PlatformUtilities.h"
#import <WebKit/WebViewPrivate.h>
#import <WebKit/DOM.h>
#import <wtf/RetainPtr.h>

@interface NavigatorLanguageDelegate : NSObject <WebFrameLoadDelegate> {
}
@end

static bool didFinishLoad;

@implementation NavigatorLanguageDelegate

- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame
{
    didFinishLoad = true;
}

@end

namespace TestWebKitAPI {

void overrideAppleLanguages(NSString *primaryLanguage)
{
    RetainPtr<NSMutableDictionary> argumentDomain = adoptNS([[[NSUserDefaults standardUserDefaults] volatileDomainForName:NSArgumentDomain] mutableCopy]);
    if (!argumentDomain)
        argumentDomain = adoptNS([[NSMutableDictionary alloc] init]);
    
    [argumentDomain addEntriesFromDictionary:@{
        @"AppleLanguages": @[primaryLanguage]
    }];
    [[NSUserDefaults standardUserDefaults] setVolatileDomain:argumentDomain.get() forName:NSArgumentDomain];

    [[NSDistributedNotificationCenter defaultCenter] postNotificationName:@"AppleLanguagePreferencesChangedNotification" object:nil userInfo:nil options:NSNotificationDeliverImmediately];

    [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];
}

static NSString *languageForSystemLanguage(WebView* webView, NSString *systemLanguage)
{
    overrideAppleLanguages(systemLanguage);
    return [webView stringByEvaluatingJavaScriptFromString:@"navigator.language"];
}

// These tests document current behavior. Some of the current results may not be right. Note that
// this oddly assumes that the user has set their language to something possibly-foreign but still
// left their region as US. Hence the "-us" variants.
// FIXME: These tests should also set the region to see how WebKit will handle that.
// https://bugs.webkit.org/show_bug.cgi?id=157039
NSArray *tests = @[
    @[@"ru", @"ru-us"], // This does not match other browsers or CFNetwork's Accept-Language, which all use "ru".
    @[@"en", @"en-us"],
    @[@"en-GB", @"en-gb"],
    @[@"en-US", @"en-us"],
    @[@"ja", @"ja-us"],
    @[@"hi", @"hi-us"],
    @[@"zh-TW", @"zh-tw"], // This should not map to the generic zh-hant, see rdar://problem/21395180.
    @[@"zh-HK", @"zh-tw"],
    @[@"es", @"es-us"],
    @[@"es-MX", @"es-xl"],
    @[@"es-ES", @"es-es"],
    @[@"es-419", @"es-xl"],
    @[@"zh-Hans", @"zh-cn"],
    @[@"zh-Hant", @"zh-tw"],
    @[@"pt-BR", @"pt-br"],
    @[@"pt-PT", @"pt-pt"],
    @[@"fr", @"fr-us"],
    @[@"fr-CA", @"fr-ca"],
];

TEST(WebKit1, NavigatorLanguage)
{
    RetainPtr<WebView> webView = adoptNS([[WebView alloc] initWithFrame:NSZeroRect frameName:nil groupName:nil]);
    RetainPtr<NavigatorLanguageDelegate> frameLoadDelegate = adoptNS([NavigatorLanguageDelegate new]);

    webView.get().frameLoadDelegate = frameLoadDelegate.get();
    [[webView.get() mainFrame] loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]]];

    Util::run(&didFinishLoad);
    for (NSArray *test in tests)
        EXPECT_WK_STREQ([test objectAtIndex:1], languageForSystemLanguage(webView.get(), [test objectAtIndex:0]));
}

#if WK_API_ENABLED

TEST(WKWebView, NavigatorLanguage)
{
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]];
    [webView loadRequest:request];
    __block bool isDone = false;

    __block void (^runTest)(NSUInteger) = ^(NSUInteger index) {
        NSArray *test = [tests objectAtIndex:index];
        overrideAppleLanguages([test objectAtIndex:0]);
        [webView evaluateJavaScript:@"navigator.language" completionHandler:^(id result, NSError *) {
            EXPECT_WK_STREQ([test objectAtIndex:1], result);
            if (index + 1 < [tests count])
                runTest(index + 1);
            else
                isDone = true;
        }];
    };

    runTest(0);

    TestWebKitAPI::Util::run(&isDone);
}

#endif

} // namespace TestWebKitAPI
