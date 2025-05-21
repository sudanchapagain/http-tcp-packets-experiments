use reqwest::blocking::Client;
use serde_json::Value;
use std::env;

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() != 2 {
        eprintln!("Usage: github-activity <username>");
        return;
    }

    let username = &args[1];
    let api_url = format!("https://api.github.com/users/{}/events", username);

    let client = Client::new();

    let response = match client
        .get(&api_url)
        .header("User-Agent", "github-activity-rust")
        .send()
    {
        Ok(resp) => resp,
        Err(e) => {
            eprintln!("Request failed: {}", e);
            return;
        }
    };

    if !response.status().is_success() {
        println!(
            "Error: Could not fetch activity for user '{}'. Response code: {}",
            username,
            response.status()
        );
        return;
    }

    let json_response: Value = match response.json() {
        Ok(json) => json,
        Err(e) => {
            eprintln!("Failed to parse JSON response: {}", e);
            return;
        }
    };

    if !json_response.is_array() {
        if let Some(error_message) = json_response["message"].as_str() {
            println!("Error: {}", error_message);
        } else {
            println!("Unknown error occurred");
        }
        return;
    }

    let events = json_response.as_array().unwrap();
    for event in events {
        let event_type = event["type"].as_str().unwrap_or("Unknown");
        let repo_name = event["repo"]["name"].as_str().unwrap_or("Unknown");

        match event_type {
            "PushEvent" => {
                let commit_count = event["payload"]["size"].as_i64().unwrap_or(0);
                println!("- Pushed {} commits to {}", commit_count, repo_name);
            }
            "IssuesEvent" => {
                let action = event["payload"]["action"].as_str().unwrap_or("");
                if action == "opened" {
                    println!("- Opened a new issue in {}", repo_name);
                } else if action == "closed" {
                    println!("- Closed an issue in {}", repo_name);
                }
            }
            "WatchEvent" => {
                println!("- Starred {}", repo_name);
            }
            _ => {
                println!("- Unhandled event type: {}", event_type);
            }
        }
    }
}
