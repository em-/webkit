var inWorker = false;
var RESOURCES_DIR = "../resources/";

try {
  inWorker = !(self instanceof Window);
} catch (e) {
  inWorker = true;
}

function dirname(path) {
    return path.replace(/\/[^\/]*$/, '/')
}

function checkRequest(request, ExpectedValuesDict) {
  for (var attribute in ExpectedValuesDict) {
    switch(attribute) {
      case "headers":
        for (var key in ExpectedValuesDict["headers"].keys()) {
          assert_equals(request["headers"].get(key), ExpectedValuesDict["headers"].get(key),
            "Check headers attribute has " + key + ":" + ExpectedValuesDict["headers"].get(key));
        }
        break;

      case "body":
        //for checking body's content, a dedicated asyncronous/promise test should be used
        assert_true(request["headers"].has("Content-Type") , "Check request has body using Content-Type header")
        break;

      case "method":
      case "referrer":
      case "referrerPolicy":
      case "credentials":
      case "cache":
      case "redirect":
      case "integrity":
      case "url":
      case "destination":
        assert_equals(request[attribute], ExpectedValuesDict[attribute], "Check " + attribute + " attribute")
        break;

      default:
        break;
    }
  }
}

function stringToArray(str) {
  var array = new Uint8Array(str.length);
  for (var i=0, strLen = str.length; i < strLen; i++)
    array[i] = str.charCodeAt(i);
  return array;
}

function validateBufferFromString(buffer, expectedValue, message)
{
  return assert_array_equals(new Uint8Array(buffer), stringToArray(expectedValue), message);
}

function validateStreamFromString(reader, expectedValue, retrievedArrayBuffer) {
  return reader.read().then(function(data) {
    if (!data.done) {
      var newBuffer;
      if (retrievedArrayBuffer) {
        newBuffer =  new ArrayBuffer(data.value.byteLength + retrievedArrayBuffer.byteLength);
        newBuffer.set(retrievedArrayBuffer, 0);
        newBuffer.set(data.value, retrievedArrayBuffer.byteLength);
      } else {
        newBuffer = data.value;
      }
      return validateStreamFromString(reader, expectedValue, newBuffer);
    }
    if (!retrievedArrayBuffer)
        retrievedArrayBuffer = new Uint8Array();
    validateBufferFromString(retrievedArrayBuffer, expectedValue, "Retrieve and verify stream");
  });
}

// From streams tests
function delay(milliseconds)
{
    return new Promise(function(resolve) {
        step_timeout(resolve, milliseconds);
    });
}
