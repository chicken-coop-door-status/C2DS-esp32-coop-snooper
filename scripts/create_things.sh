#!/bin/bash

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
done
