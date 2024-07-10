#!/bin/bash

# Function to generate IoT policy
generate_iot_policy() {
  local account_number=$(aws sts get-caller-identity --query "Account" --profile tennis@charliesfarm --region us-east-2 --output text)
  local region="us-east-2"
  local resource_name="coop-snooper"

  local topics=("coop/status" "coop/status/request" "coop/led/color" "coop/update/snooper")

  local policy=$(cat <<EOF
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Effect": "Allow",
      "Action": "iot:Connect",
      "Resource": [
        "arn:aws:iot:$region:$account_number:client/$resource_name",
        "arn:aws:iot:$region:$account_number:client/*"
      ]
    },
    {
      "Effect": "Allow",
      "Action": [
        "iot:Publish",
        "iot:Receive",
        "iot:Subscribe"
      ],
      "Resource": [
EOF
)

  # Add topics to the policy
  for topic in "${topics[@]}"; do
    policy+=$(cat <<EOF
        "arn:aws:iot:$region:$account_number:topic/$topic",
        "arn:aws:iot:$region:$account_number:topicfilter/$topic",
EOF
)
  done

  # Remove the trailing comma
  policy=${policy%,}

  # Close the policy JSON
  policy+=$(cat <<EOF
      ]
    }
  ]
}
EOF
)

  echo "$policy" | jq . 
}

# Function to create a certificate and attach it to a thing
create_and_attach_cert() {
  local thing=$1

  # Create certificate and capture the response
  response=$(aws iot create-keys-and-certificate \
    --set-as-active \
    --output json \
    --profile tennis@charliesfarm \
    --region us-east-2
  )
  
  # Check if the response is valid JSON
  if ! echo "$response" | jq empty; then
    echo "Failed to create certificate for $thing"
    return
  fi

  # Extract certificate ARN and save certificate files
  certificateArn=$(echo $response | jq -r '.certificateArn')
  certificatePem=$(echo $response | jq -r '.certificatePem')
  publicKey=$(echo $response | jq -r '.keyPair.PublicKey')
  privateKey=$(echo $response | jq -r '.keyPair.PrivateKey')
  certificateId=$(echo $response | jq -r '.certificateId')

  # Save certificates to files
  echo "$certificatePem" > "$thing-certificate.pem"
  echo "$publicKey" > "$thing-public.pem.key"
  echo "$privateKey" > "$thing-private.pem.key"

  # Attach certificate to the Thing
  aws iot attach-thing-principal \
    --thing-name "$thing" \
    --principal "$certificateArn" \
    --profile tennis@charliesfarm \
    --region us-east-2

  generate_iot_policy > "${thing}-iot-policy.json"

  aws iot create-policy \
    --policy-name "${thing}-iot-policy" \
    --policy-document file://${thing}-iot-policy.json \
    --profile tennis@charliesfarm \
    --region us-east-2

  aws iot attach-policy \
    --policy-name "${thing}-iot-policy" \
    --target "$certificateArn" \
    --profile tennis@charliesfarm \
    --region us-east-2

  echo "Generated and attached certificate for $thing"
}

aws iot create-thing-type \
    --thing-type-name "coop-snooper" \
    --profile tennis@charliesfarm \
    --region us-east-2

# Read the JSON file
things=$(jq -c '.things[]' things.json)

# Loop through each thing and create it
for thing in $things; do
  thingName=$(echo $thing | jq -r '.thingName')
  thingTypeName=$(echo $thing | jq -r '.thingTypeName')
  attributes=$(echo $thing | jq -r '.attributePayload.attributes | tojson')
  
  aws iot create-thing \
    --thing-name $thingName \
    --thing-type-name $thingTypeName \
    --attribute-payload "{\"attributes\":$attributes}" \
    --profile tennis@charliesfarm \
    --region us-east-2
  
  echo "Created thing: $thingName"

  create_and_attach_cert "$thingName"
done