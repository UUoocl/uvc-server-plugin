#import <Foundation/Foundation.h>
#import "UVCController.h"
#import "UVCValue.h"

// Global state
static NSArray *g_uvcDevices = nil;
static UVCController *g_currentDevice = nil;
static NSString *g_lastResultString = nil;

// Helper to set the result string and return its C-string pointer
const char *set_result_string(NSString *str)
{
    if (g_lastResultString) {
        [g_lastResultString release];
        g_lastResultString = nil;
    }
    if (str) {
        g_lastResultString = [str copy];
        return [g_lastResultString UTF8String];
    }
    return NULL;
}

// Helper to set result from JSON object
const char *set_result_json(id object)
{
    if (!object)
        return set_result_string(nil);

    NSError *error = nil;
    NSData *jsonData = [NSJSONSerialization dataWithJSONObject:object options:0 error:&error];
    if (jsonData) {
        NSString *jsonString = [[NSString alloc] initWithData:jsonData encoding:NSUTF8StringEncoding];
        const char *res = set_result_string(jsonString);
        [jsonString release];
        return res;
    }
    return set_result_string(@"{\"error\": \"JSON serialization failed\"}");
}

// --- API Functions ---

// Initialize / Refresh device list
int uvclib_refresh_devices()
{
    if (g_uvcDevices) {
        [g_uvcDevices release];
        g_uvcDevices = nil;
    }
    g_uvcDevices = [[UVCController uvcControllers] retain];
    return (int) [g_uvcDevices count];
}

// Get list of devices as JSON
const char *uvclib_get_devices_json()
{
    if (!g_uvcDevices)
        uvclib_refresh_devices();

    NSMutableArray *devList = [NSMutableArray array];
    unsigned long deviceIndex = 0;

    for (UVCController *device in g_uvcDevices) {
        UInt16 uvcVersion = [device uvcVersion];
        NSString *verStr = [NSString stringWithFormat:@"%d.%02x", (short) (uvcVersion >> 8), (uvcVersion & 0xFF)];

        NSDictionary *d = [NSDictionary
            dictionaryWithObjectsAndKeys:[NSNumber numberWithUnsignedLong:deviceIndex++], @"index",
                                         [NSNumber numberWithUnsignedShort:[device vendorId]], @"vendorId",
                                         [NSNumber numberWithUnsignedShort:[device productId]], @"productId",
                                         [NSNumber numberWithUnsignedInt:[device locationId]], @"locationId", verStr,
                                         @"uvcVersion", [device deviceName], @"name", nil];
        [devList addObject:d];
    }
    return set_result_json(devList);
}

// Select device by index
int uvclib_select_device(unsigned int index)
{
    if (g_uvcDevices && index < [g_uvcDevices count]) {
        if (g_currentDevice != [g_uvcDevices objectAtIndex:index]) {
            g_currentDevice = [g_uvcDevices objectAtIndex:index];
        }
        return 0;  // Success
    }
    return -1;  // Error
}

// Get controls for selected device
const char *uvclib_get_controls_json()
{
    if (!g_currentDevice)
        return set_result_string(@"[]");

    NSArray *controlNames = [UVCController controlStrings];
    NSMutableArray *list = [NSMutableArray array];

    if (controlNames) {
        for (NSString *name in controlNames) {
            UVCControl *control = [g_currentDevice controlWithName:name];
            if (control) {
                [list addObject:[control summaryDictionary]];
            }
        }
    }
    return set_result_json(list);
}

// Get value of a specific control
const char *uvclib_get_value(const char *control_name)
{
    if (!g_currentDevice)
        return set_result_string(@"{\"error\": \"No device selected\"}");

    NSString *nameStr = [NSString stringWithUTF8String:control_name];
    UVCControl *control = [g_currentDevice controlWithName:nameStr];

    if (control) {
        UVCValue *currentValue = [control currentValue];
        if (currentValue) {
            NSMutableDictionary *res = [NSMutableDictionary dictionary];
            [res setObject:[control controlName] forKey:@"control"];
            [res setObject:[currentValue jsonObject] forKey:@"value"];
            return set_result_json(res);
        }
    }
    return set_result_string(@"{\"error\": \"Control not found or readable\"}");
}

// Set value of a control
const char *uvclib_set_value(const char *control_name, const char *value_str)
{
    if (!g_currentDevice)
        return set_result_string(@"{\"error\": \"No device selected\"}");

    NSString *nameStr = [NSString stringWithUTF8String:control_name];
    UVCControl *control = [g_currentDevice controlWithName:nameStr];

    if (control) {
        if ([control setCurrentValueFromCString:value_str flags:0]) {
            if ([control writeFromCurrentValue]) {
                NSMutableDictionary *res = [NSMutableDictionary dictionary];
                [res setObject:@"success" forKey:@"status"];
                [res setObject:[control controlName] forKey:@"control"];
                if ([control currentValue]) {
                    [res setObject:[[control currentValue] jsonObject] forKey:@"new-value"];
                }
                return set_result_json(res);
            } else {
                return set_result_string([NSString stringWithFormat:@"{\"error\": \"Write failed for %@\"}", nameStr]);
            }
        } else {
            return set_result_string(
                [NSString stringWithFormat:@"{\"error\": \"Invalid value format '%s' for %@\"}", value_str, nameStr]);
        }
    }
    return set_result_string(@"{\"error\": \"Control not found\"}");
}
