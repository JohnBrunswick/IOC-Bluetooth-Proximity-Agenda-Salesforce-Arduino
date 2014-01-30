/*
  Node.js application running on Heroku that connects to Salesforce.com
  using the nForce package to query support information for a customer based
  on an RFID custom field for an account.  The application then uses Socket.IO
  to update any clients browing a dashboard at http://localhost:3001

  Resources used
  nForce package - https://github.com/kevinohara80/nforce nforce is a node.js salesforce REST API wrapper for force.com, database.com, and salesforce.com.  Thanks to https://twitter.com/kevino80
  Express web application framework - http://expressjs.com/
  Socket.IO web socket library - http://socket.io/

  more details at www.johnbrunswick.com
*/
var express=require('express');
var nforce = require('nforce');

var app = express()
  , http = require('http')
  , server = http.createServer(app)
  , io = require('socket.io').listen(server);

// the env.PORT values are stored in the .env file, as well as authentication information needed for nForce
var port = process.env.PORT || 3001; // use port 3001 if localhost (e.g. http://localhost:3001)
var oauth;

// make sure we can only greet one user at a time
var readytospeak = 1;

// track the last guest, so we do not repeat welcomes
var lastguest = '';

// track who has been greated
var guests = new Array();

// configuration
app.use(express.bodyParser());
app.use(express.methodOverride());
app.use(express.static(__dirname + '/public'));  

server.listen(port);

// dashboard.html will be sent to requesting user at root - http://localhost:3001
app.get('/', function (req, res) {
  res.sendfile(__dirname + '/views/dashboard.html');
});

// use the nforce package to create a connection to salesforce.com
var org = nforce.createConnection({
  clientId: process.env.CLIENT_ID,
  clientSecret: process.env.CLIENT_SECRET,
  redirectUri: 'http://localhost:' + port + '/oauth/_callback',
  apiVersion: 'v24.0',  // optional, defaults to v24.0
});

// authenticate using username-password oauth flow
org.authenticate({ username: process.env.USERNAME, password: process.env.PASSWORD }, function(err, resp){
  if(err) {
    console.log('Error: ' + err.message);
  } else {
    console.log('Access Token: ' + resp.access_token);
    oauth = resp;
  }
});

app.put('/checkin', function(req, res) {
  console.log('PUT received');
  var btid = req.body.mobiledevice.btid;

  console.log("Last guest: " + lastguest);
  console.log("New guest : " + btid)

  // check to see if speech is already in progress
  if (readytospeak == 1)
  {

    // check to make sure we do not greet twice
//    if (guests.indexOf(btid) < 0) {
    if (btid != "001FF3AD6525,380104") {
      guests.push(btid);

      // lock the speaking, so we only address 1 guest at a time
      readytospeak = 0

      console.log("Guests so far - ");
      //console.log(btid);

      guests.forEach(function(entry) {
          console.log("Guest: " + entry);
      });

    //  var btid = "A80600BE035D,5A020C";
    //  var btid = "A80600BE035D,5A0201";

      var taskspeech = '';
      var greetinghour = '';
      var greetinghour = 'afternoon'; // could be dynamic based on time and SFDC user timezone
      var greetingname = '';
      var speechvoice = '';

      // Get user info for photo
      var quser = "SELECT Id, FirstName, LastName, Profile_Photo__c, Greeting_Voice__c FROM User WHERE Phone_Code__c = '" + btid + "'";

      org.query(quser, oauth, function(err, resp){
        if(!err && resp.records) {
            // get speech defaults used for audio greeting
            greetingname = resp.records[0].FirstName;
            speechvoice = resp.records[0].Greeting_Voice__c;

            io.sockets.emit("proximity", resp.records[0]);

            console.log("greetingname: " + greetingname);
            console.log("speechvoice: " + speechvoice);


            // User the MAC address from the Bluetooth
            var q = "SELECT Id, OwnerId, Description FROM Task WHERE OwnerId IN (SELECT Id FROM User WHERE  Phone_Code__c = '" + btid + "') AND Status = 'Not Started'";

            org.query(q, oauth, function(err, resp){
              if(!err && resp.records) {

                var opentasks = resp.records;

                var say = require('say'),
                colors = require('colors'),
                sys = require('sys');

                var plural = '';
                if (opentasks.length > 1)
                {
                  plural = 's';
                }

                // no callback, fire and forget
                say.speak(speechvoice, "Good " + greetinghour + "  " + greetingname + ". You currently have " + opentasks.length + " task" + plural + ".", function () {

                  var taskcount = 0;

                  opentasks.forEach(function(item) { 
                    taskcount += 1;

                    // commas add a pause during the speech
                    taskspeech += "Task " + taskcount + ", " + item.Description + ", ";
                  })

                  // no callback, fire and forget
                  say.speak(speechvoice, taskspeech, function () {
                    // close the welcome photo on the client side
                    io.sockets.emit("closephoto", true);

                    // ready to greet another guest
                    readytospeak = 1;
                  });

                });
              } 
            });
        } 
      });  
}
/*    }
    else {
      console.log(btid + " has already been greeted!");
    }*/
  }
  else{
     console.log("Speaking is already in process...");
  }

  res.send({});
});