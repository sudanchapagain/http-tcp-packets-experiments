package np.com.sudanchapagain;

import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonParser;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.URI;
import java.net.URL;

public class Main {
    public static void main(String[] args) {
        if (args.length != 1) {
            System.out.println("Usage: github-activity <username>");
            return;
        }

        String username = args[0];
        String apiUrl = "https://api.github.com/users/" + username + "/events";

        try {
            URI uri = new URI(apiUrl);
            URL url = uri.toURL();

            HttpURLConnection connection = (HttpURLConnection) url.openConnection();
            connection.setRequestMethod("GET");

            int responseCode = connection.getResponseCode();

            if (responseCode == HttpURLConnection.HTTP_OK) {
                BufferedReader in = new BufferedReader(new InputStreamReader(connection.getInputStream()));
                StringBuilder response = new StringBuilder();
                String inputLine;

                while ((inputLine = in.readLine()) != null) {
                    response.append(inputLine);
                }
                in.close();

                JsonElement jsonResponse = JsonParser.parseString(response.toString());

                if (jsonResponse.isJsonArray()) {
                    JsonArray events = jsonResponse.getAsJsonArray();

                    for (JsonElement event : events) {
                        JsonObject eventObject = event.getAsJsonObject();
                        String type = eventObject.get("type").getAsString();
                        JsonObject repo = eventObject.getAsJsonObject("repo");
                        String repoName = repo.get("name").getAsString();

                        switch (type) {
                            case "PushEvent":
                                int commitCount = eventObject.getAsJsonObject("payload").getAsJsonObject().get("size").getAsInt();
                                System.out.println("- Pushed " + commitCount + " commits to " + repoName);
                                break;

                            case "IssuesEvent":
                                String action = eventObject.getAsJsonObject("payload").get("action").getAsString();
                                if (action.equals("opened")) {
                                    System.out.println("- Opened a new issue in " + repoName);
                                } else if (action.equals("closed")) {
                                    System.out.println("- Closed an issue in " + repoName);
                                }
                                break;

                            case "WatchEvent":
                                System.out.println("- Starred " + repoName);
                                break;

                            default:
                                System.out.println("- Unhandled event type: " + type);
                                break;
                        }
                    }
                } else if (jsonResponse.isJsonObject()) {
                    JsonObject errorObject = jsonResponse.getAsJsonObject();
                    System.out.println("Error: " + errorObject.get("message").getAsString());
                }
            } else {
                System.out.println("Error: Could not fetch activity for user '" + username + "'. Response code: " + responseCode);
            }
        } catch (Exception e) {
            System.out.println("Error: " + e.getMessage());
        }
    }
}
